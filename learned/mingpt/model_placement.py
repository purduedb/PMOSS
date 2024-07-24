"""
GPT model:
- the initial stem consists of a combination of token encoding and a positional encoding
- the meat of it is a uniform sequence of Transformer blocks
    - each Transformer is a sequential combination of a 1-hidden-layer MLP block and a self-attention block
    - all blocks feed into a central residual pathway similar to resnets
- the final decoder is a linear projection into a vanilla Softmax classifier
"""

import math
import random
import logging

import torch
import torch.nn as nn
from torch.nn import functional as F
torch.set_printoptions(threshold=1000000)
logger = logging.getLogger(__name__)

from create_dataset import test_returns
grid = 8  #=> my-
nFeatures = 5 # 5, 23, 10
no_circuit_emb = False
machine = 1

benchmark_to_id = {'adaptec1': 0, 'adaptec2': 1, 'adaptec3': 2, 'adaptec4': 3,
            'bigblue1': 4, 'bigblue2': 5, 'bigblue3': 6, 'bigblue4': 7,
            'ibm01': 8, 'ibm02': 9, 'ibm03': 10, 'ibm04': 11,
            'ibm05':12, 'ibm06':13,}
benchmark_id_to_name = {v: k for k, v in benchmark_to_id.items()}

class GELU(nn.Module):
    def forward(self, input):
        return F.gelu(input)

class GPTConfig:
    """ base GPT config, params common to all GPT versions """
    embd_pdrop = 0.1
    resid_pdrop = 0.1
    attn_pdrop = 0.1

    def __init__(self, vocab_size, block_size, **kwargs):
        self.vocab_size = vocab_size  # int((grid) ** 2)
        self.block_size = block_size  # 3 * context_size =  seq_length
        for k,v in kwargs.items():
            setattr(self, k, v)

class GPT1Config(GPTConfig):
    """ GPT-1 like network roughly 125M params """
    n_layer = 12
    n_head = 12
    n_embd = 768

class CausalSelfAttention(nn.Module):
    """
    A vanilla multi-head masked self-attention layer with a projection at the end.
    It is possible to use torch.nn.MultiheadAttention here but I am including an
    explicit implementation here to show that there is nothing too scary here.
    """

    def __init__(self, config):
        super().__init__()
        assert config.n_embd % config.n_head == 0
        # key, query, value projections for all heads
        self.key = nn.Linear(config.n_embd, config.n_embd)
        self.query = nn.Linear(config.n_embd, config.n_embd)
        self.value = nn.Linear(config.n_embd, config.n_embd)
        # regularization
        self.attn_drop = nn.Dropout(config.attn_pdrop)
        self.resid_drop = nn.Dropout(config.resid_pdrop)
        # output projection
        self.proj = nn.Linear(config.n_embd, config.n_embd)
        # causal mask to ensure that attention is only applied to the left in the input sequence
        self.register_buffer("mask", torch.tril(torch.ones(config.block_size + 1, config.block_size + 1))
                                     .view(1, 1, config.block_size + 1, config.block_size + 1))
        self.n_head = config.n_head

    def forward(self, x, layer_past=None):
        B, T, C = x.size()

        # calculate query, key, values for all heads in batch and move head forward to be the batch dim
        k = self.key(x).view(B, T, self.n_head, C // self.n_head).transpose(1, 2) # (B, nh, T, hs)
        q = self.query(x).view(B, T, self.n_head, C // self.n_head).transpose(1, 2) # (B, nh, T, hs)
        v = self.value(x).view(B, T, self.n_head, C // self.n_head).transpose(1, 2) # (B, nh, T, hs)

        # causal self-attention; Self-attend: (B, nh, T, hs) x (B, nh, hs, T) -> (B, nh, T, T)
        att = (q @ k.transpose(-2, -1)) * (1.0 / math.sqrt(k.size(-1)))
        att = att.masked_fill(self.mask[:,:,:T,:T] == 0, float('-inf'))
        att = F.softmax(att, dim=-1)
        att = self.attn_drop(att)
        y = att @ v # (B, nh, T, T) x (B, nh, T, hs) -> (B, nh, T, hs)
        y = y.transpose(1, 2).contiguous().view(B, T, C) # re-assemble all head outputs side by side

        # output projection
        y = self.resid_drop(self.proj(y))
        return y

class Block(nn.Module):
    """ an unassuming Transformer block """

    def __init__(self, config):
        super().__init__()
        self.ln1 = nn.LayerNorm(config.n_embd)
        self.ln2 = nn.LayerNorm(config.n_embd)
        self.attn = CausalSelfAttention(config)
        self.mlp = nn.Sequential(
            nn.Linear(config.n_embd, 4 * config.n_embd),
            GELU(),
            nn.Linear(4 * config.n_embd, config.n_embd),
            nn.Dropout(config.resid_pdrop),
        )

    def forward(self, x):
        x = x + self.attn(self.ln1(x))
        x = x + self.mlp(self.ln2(x))
        return x


class Reshape(nn.Module):
    def __init__(self, *args):
        super(Reshape, self).__init__()
        self.shape = args
    def forward(self, x):
        return x.view((x.size(0),)+self.shape)


class GPT(nn.Module):
    """  the full GPT language model, with a context size of block_size """

    def __init__(self, config):
        super().__init__()

        self.config = config
        self.model_type = config.model_type  # str: "reward_conditioned"

        # input embedding stem
        print("config.vocab_size", config.vocab_size)  
        
        self.tok_emb = nn.Embedding(config.vocab_size, config.n_embd)  # (grid^2, Your choice = 128) 
        # Your dictionary has [vocab_size] words and each word is of size [n_embd]
        # A simple lookup table

        #  255*3(rtg, state, action) + 1 
        # => my (1, 3*256 +1, 128)
        self.pos_emb = nn.Parameter(torch.zeros(1, config.block_size + 1, config.n_embd)) # 255*3  
        #  255 + 1(circuit emb)
        # => my (1, 256 +1, 128)
        self.global_pos_emb = nn.Parameter(torch.zeros(1, config.block_size // 3 + 1, config.n_embd))  # => my (1, 256 +1, 128)
        
        self.drop = nn.Dropout(config.embd_pdrop)

        # transformer
        self.blocks = nn.Sequential(*[Block(config) for _ in range(config.n_layer)])
        
        # decoder head
        self.ln_f = nn.LayerNorm(config.n_embd)
        self.head = nn.Linear(config.n_embd, config.vocab_size, bias=False)
        self.block_size = config.block_size
        self.apply(self._init_weights)


        logger.info("number of parameters: %e", sum(p.numel() for p in self.parameters()))

        # -------------------------------------------------------------------------------------
        # self.state_encoder_s = nn.Sequential(nn.Conv2d(3, 16, 8, stride=2, padding=1), nn.ReLU(), # 
        #                          nn.Conv2d(16, 32, 4, stride=2, padding=1), nn.ReLU(), 
        #                          nn.Conv2d(32, 16, 3, stride=2, padding=1), nn.ReLU(), # 14*14*16
        #                          nn.Flatten(), nn.Linear(1600, config.n_embd-2))
        #=> my changed the first convolution from [(3, 16, 8)] to [(8, 16, 8)]
        
        # => f (my) July
        # self.state_encoder_s = nn.Sequential(nn.Conv2d(8, 16, 8, stride=2, padding=1), nn.ReLU(), # 
        #                          nn.Conv2d(16, 32, 4, stride=2, padding=1), nn.ReLU(), 
        #                          nn.Conv2d(32, 16, 3, stride=2, padding=1), nn.ReLU(), # 14*14*16
        #                          nn.Flatten(), nn.Linear(16, config.n_embd-16))  # Added -16 to incorporate meta data
        if machine == 0:
            self.state_encoder_s = nn.Sequential(nn.Conv2d(3+nFeatures, 16, 8, stride=2, padding=1), nn.ReLU(), # 
                                 nn.Conv2d(16, 32, 4, stride=2, padding=1), nn.ReLU(), 
                                 nn.Conv2d(32, 16, 3, stride=2, padding=1), nn.ReLU(), # 14*14*16
                                 nn.Flatten(), nn.Linear(16, config.n_embd-16))  # Added -16 to incorporate meta data
        else:
            self.state_encoder_s = nn.Sequential(nn.Conv2d(3+nFeatures, 16, 8, stride=2, padding=1), nn.ReLU(), # 
                                 nn.Conv2d(16, 32, 4, stride=2, padding=1), nn.ReLU(), 
                                 nn.Conv2d(32, 16, 3, stride=2, padding=1), nn.ReLU(), # 14*14*16
                                 nn.Flatten(), nn.Linear(16, config.n_embd-4))  # Added -16 to incorporate meta data

        # => f (my)
        # self.state_encoder_s = nn.Sequential(nn.Conv2d(26, 16, 8, stride=2, padding=1), nn.ReLU(), # 
        #                          nn.Conv2d(16, 32, 4, stride=2, padding=1), nn.ReLU(), 
        #                          nn.Conv2d(32, 16, 3, stride=2, padding=1), nn.ReLU(), # 14*14*16
        #                          nn.Flatten(), nn.Linear(16, config.n_embd-16))  # Added -16 to incorporate meta data
        if machine == 0:
            self.meta_encoder_s = nn.Sequential(nn.Linear(16, 32), nn.ReLU(), nn.Linear(32, 16))
        else:
            self.meta_encoder_s = nn.Sequential(nn.Linear(4, 32), nn.ReLU(), nn.Linear(32, 4))

        #=> h/w (my)
        # self.state_encoder_s = nn.Sequential(nn.Conv2d(2, 16, 8, stride=2, padding=1), nn.ReLU(), # 
        #                          nn.Conv2d(16, 32, 4, stride=2, padding=1), nn.ReLU(), 
        #                          nn.Conv2d(32, 16, 3, stride=2, padding=1), nn.ReLU(), # 14*14*16
        #                          nn.Flatten(), nn.Linear(16, config.n_embd))
        # -------------------------------------------------------------------------------------
        
        # print(self.state_encoder_s)
        # small kernel conv
        # => my July
        self.action_head = nn.Sequential(nn.Conv2d(3, 3+nFeatures, 1, stride=1, padding=0), nn.ReLU(), # 
                                 nn.Conv2d(3+nFeatures, 8, 1, stride=1, padding=0), nn.ReLU(), 
                                 nn.Conv2d(8, 1, 1, stride=1, padding=0), # 14*14*8
                                 nn.Flatten())
        self.action_head_s = nn.Sequential(nn.Conv2d(1, 3+nFeatures, 1, stride=1, padding=0), nn.ReLU(), # 
                                 nn.Conv2d(3+nFeatures, 8, 1, stride=1, padding=0), nn.ReLU(), 
                                 nn.Conv2d(8, 1, 1, stride=1, padding=0), # 14*14*8
                                 nn.Flatten())
        # self.action_head = nn.Sequential(nn.Conv2d(3, 8, 1, stride=1, padding=0), nn.ReLU(), # 
        #                          nn.Conv2d(8, 8, 1, stride=1, padding=0), nn.ReLU(), 
        #                          nn.Conv2d(8, 1, 1, stride=1, padding=0), # 14*14*8
        #                          nn.Flatten())
        # self.action_head_s = nn.Sequential(nn.Conv2d(1, 8, 1, stride=1, padding=0), nn.ReLU(), # 
        #                          nn.Conv2d(8, 8, 1, stride=1, padding=0), nn.ReLU(), 
        #                          nn.Conv2d(8, 1, 1, stride=1, padding=0), # 14*14*8
        #                          nn.Flatten())

        self.one_kernel = nn.Sequential(nn.Conv2d(2, 1, 1, stride=1, padding=0), nn.Flatten())

        self.ret_emb = nn.Sequential(nn.Linear(1, config.n_embd), nn.Tanh())
        
        self.circuit_emb = nn.Embedding(10, config.n_embd-2)

        self.circuit_emb_tmp = nn.Embedding(20, config.n_embd)

        self.circuit_emb_s = nn.Linear(2, config.n_embd)

        self.circuit_emb_ss = nn.Sequential(nn.Linear(256*3, 1024), nn.ReLU(), 
                                nn.Linear(1024, 1024), nn.ReLU(),
                                nn.Linear(1024, config.n_embd))
        if machine == 0:
            self.action_embeddings_s = nn.Embedding(grid*grid, config.n_embd)
        else:
            self.action_embeddings_s = nn.Embedding(8*10, config.n_embd)

    def get_block_size(self):
        return self.block_size

    def _init_weights(self, module):
        if isinstance(module, (nn.Linear, nn.Embedding)):
            module.weight.data.normal_(mean=0.0, std=0.02)
            if isinstance(module, nn.Linear) and module.bias is not None:
                module.bias.data.zero_()
        elif isinstance(module, nn.LayerNorm):
            module.bias.data.zero_()
            module.weight.data.fill_(1.0)

    def configure_optimizers(self, train_config):
        """
        This long function is unfortunately doing something very simple and is being very defensive:
        We are separating out all parameters of the model into two buckets: those that will experience
        weight decay for regularization and those that won't (biases, and layernorm/embedding weights).
        We are then returning the PyTorch optimizer object.
        """

        # separate out all parameters to those that will and won't experience regularizing weight decay
        decay = set()
        no_decay = set()
        # whitelist_weight_modules = (torch.nn.Linear, )
        whitelist_weight_modules = (torch.nn.Linear, torch.nn.Conv2d, torch.nn.ConvTranspose2d, torch.nn.Conv1d)
        blacklist_weight_modules = (torch.nn.LayerNorm, torch.nn.Embedding)
        for mn, m in self.named_modules():
            for pn, p in m.named_parameters():
                fpn = '%s.%s' % (mn, pn) if mn else pn # full param name

                if pn.endswith('bias'):
                    # all biases will not be decayed
                    no_decay.add(fpn)
                elif pn.endswith('weight') and isinstance(m, whitelist_weight_modules):
                    # weights of whitelist modules will be weight decayed
                    decay.add(fpn)
                elif pn.endswith('weight') and isinstance(m, blacklist_weight_modules):
                    # weights of blacklist modules will NOT be weight decayed
                    no_decay.add(fpn)

        # special case the position embedding parameter in the root GPT module as not decayed
        no_decay.add('pos_emb')
        no_decay.add('global_pos_emb')

        # validate that we considered every parameter
        param_dict = {pn: p for pn, p in self.named_parameters()}
        inter_params = decay & no_decay
        union_params = decay | no_decay
        assert len(inter_params) == 0, "parameters %s made it into both decay/no_decay sets!" % (str(inter_params), )
        assert len(param_dict.keys() - union_params) == 0, "parameters %s were not separated into either decay/no_decay set!" \
                                                    % (str(param_dict.keys() - union_params), )

        # create the pytorch optimizer object
        optim_groups = [
            {"params": [param_dict[pn] for pn in sorted(list(decay))], "weight_decay": train_config.weight_decay},
            {"params": [param_dict[pn] for pn in sorted(list(no_decay))], "weight_decay": 0.0},
        ]
        optimizer = torch.optim.AdamW(optim_groups, lr=train_config.learning_rate, betas=train_config.betas)
        return optimizer

    # state, action, and return
    def forward(self, states, actions, targets=None, rtgs=None, 
        timesteps=None, meta_states = None, benchmarks= None, stepwise_returns= None,
        circuit_feas=None, lengths = None, is_random_shuffle =False):
        # states: (batch, context-length, 4*84*84)
        # actions: (batch, context, 1)
        # targets: (batch, context, 1)
        # rtgs: (batch, context, 1)
        # timesteps: (batch, context, 1)
        # meta states: (B, context, 6)
        # benchmarks: (B, context, 1, 1)
        # stepwise-returns: (B, context, 1, 1)
        # circuit_feas: (batch, 2)  => my (batch, context*3)
        # lengths: (batch, context)
        # print(states[0])
        # print(actions[0].view(-1, ))
        # print(rtgs[0].view(-1, ))
        # tt = input()
        # print(timesteps[0].view(-1, ))
        # print(meta_states[0].view(-1, ))
        # print(stepwise_returns[0].view(-1, ))
        
        if actions is not None and len(actions.shape) == 2:
            actions = actions.unsqueeze(-1)
        if meta_states is None:
            assert False
        else:
            circuit_embeddings = circuit_feas
            # -------------------------------------------------------------------------------------
            # state_embeddings = self.state_encoder_s(
            #     states.reshape(-1, 3, grid, grid).type(torch.float32).contiguous()
            #     )
            
            # => f (my): July
            # state_embeddings = self.state_encoder_s(
            #     states.reshape(-1, 8, grid, grid).type(torch.float32).contiguous()
            #     )
            if machine == 0:
                state_embeddings = self.state_encoder_s(
                    states.reshape(-1, 3+nFeatures, grid, grid).type(torch.float32).contiguous()
                    )
            else:
                state_embeddings = self.state_encoder_s(
                    states.reshape(-1, 3+nFeatures, 8, 10).type(torch.float32).contiguous()
                    )
            
            # => f (my)
            # state_embeddings = self.state_encoder_s(
            #     states.reshape(-1, 26, grid, grid).type(torch.float32).contiguous()
            #     )
            # => hw (my)
            # state_embeddings = self.state_encoder_s(
            #     states.reshape(-1, 2, grid, grid).type(torch.float32).contiguous()
            #     )
            # -------------------------------------------------------------------------------------
            
            
            # ------------------------------------------------------------------------------------
            # Now we have meta data 
            if machine == 0:
                meta_embeddings = self.meta_encoder_s(
                    meta_states[:, :].reshape(-1, 16)
                    )
                state_embeddings = torch.cat((state_embeddings, meta_embeddings[:, :].reshape(-1, 16)), dim = 1)
            else:
                meta_embeddings = self.meta_encoder_s(
                    meta_states[:, :].reshape(-1, 4)
                )
                state_embeddings = torch.cat((state_embeddings, meta_embeddings[:, :].reshape(-1, 4)), dim = 1)
            
            # state_embeddings = torch.cat((state_embeddings, meta_embeddings[:, :].reshape(-1, 16)), dim = 1)
            #=>m y
            # state_embeddings = torch.cat((state_embeddings, meta_states[:, :].reshape(-1, 16)), dim = 1)
            

            # We donot have meta states
            # state: (batch * context, 126)
            # if len(meta_states.shape) == 2:
            #     state_embeddings = torch.cat((state_embeddings, meta_states[:, :2].reshape(-1, 2)), dim = 1)
            # else:
            #     state_embeddings = torch.cat((state_embeddings, meta_states[:, :, :2].reshape(-1, 2)), dim = 1)
            #     # refining only the first 2 meta-features
            # ------------------------------------------------------------------------------------

            # state: (batch * context, 126+2 = 128 => nn.Embedding)
            state_embeddings = nn.Tanh()(state_embeddings)
            

        # batch / sequence / n_embd
        state_embeddings = state_embeddings.reshape(states.shape[0], states.shape[1], self.config.n_embd) # (batch, context, n_embd)
        
        circuit_embeddings = circuit_embeddings.reshape(-1, 256*3)  # (batch, 3*context => probably matching the states)
        circuit_embeddings = self.circuit_emb_ss(circuit_embeddings)  # (batch, nn_embedding)
        
        # ------------------------------------------------------------------------------------- 
        # We do not have circuit embedding at this point
        no_circuit_emb = True
        if no_circuit_emb:
            circuit_embeddings = torch.zeros_like(circuit_embeddings)

        if is_random_shuffle:
            circuit_embeddings = torch.zeros_like(circuit_embeddings)
        # ------------------------------------------------------------------------------------- 
        

        if actions is not None and self.model_type == 'reward_conditioned': 
            rtg_embeddings = self.ret_emb(rtgs.type(torch.float32))  # (batch, context, n_Embedding)
            rtg_embeddings = rtg_embeddings.reshape(states.shape[0], -1, self.config.n_embd)  # (batch, context, n_Embedding)
            
            # => gone to what they are doing 
            # rtg_embeddings = torch.zeros_like(rtg_embeddings)  # (batch, context, n_Embedding)
            # Here I have uncommented this line and want to see what happens?
            # Why are rtg_embeddings all zero?
            
            action_embeddings = self.action_embeddings_s(actions.type(torch.long).squeeze(-1))  # (batch, context, n_Embedding)
            
            

            # -------------------------------------------------------------------------------------
            # Creating token embedding from: [circuit-features], []
            # (b, 3*context+1, n_Embd)
            # Commented this for excluding the circuit features
            # =>my

            # token_embeddings = torch.zeros((states.shape[0], 1+states.shape[1]*3 - int(targets is None), self.config.n_embd), dtype=torch.float32, device=state_embeddings.device)
            # # A single batch looks like this [circuit][rtg_0][s_0][a_0][rtg_1][s_1][a_1]
            # token_embeddings[:,0,:] = circuit_embeddings.squeeze()   # (bs, 0, n_Embedding)
            # token_embeddings[:,1::3,:] = rtg_embeddings   
            # token_embeddings[:,2::3,:] = state_embeddings
            # token_embeddings[:,3::3,:] = action_embeddings[:,-states.shape[1] + int(targets is None):,:]
            


            # If you do not have any circuit embeddings then you can use this
            token_embeddings = torch.zeros((states.shape[0], states.shape[1]*3 - int(targets is None), self.config.n_embd), dtype=torch.float32, device=state_embeddings.device)
            token_embeddings[:,0::3,:] = rtg_embeddings   
            token_embeddings[:,1::3,:] = state_embeddings
            token_embeddings[:,2::3,:] = action_embeddings[:,-states.shape[1] + int(targets is None):,:]
            
            
        elif actions is None and self.model_type == 'reward_conditioned': # only happens at very first timestep of evaluation
            print("ENTERED HERE ASSERTION ERROR!-------------------------------------------------------------------------------------")
            rtg_embeddings = self.ret_emb(rtgs.type(torch.float32))
            rtg_embeddings = torch.zeros_like(rtg_embeddings)
            rtg_embeddings = rtg_embeddings.reshape(states.shape[0], -1, self.config.n_embd)

            token_embeddings = torch.zeros((states.shape[0], states.shape[1]*2, self.config.n_embd), dtype=torch.float32, device=state_embeddings.device)
            # we don not have any circuit embedding
            # token_embeddings[:,0::3,:] = circuit_embeddings
            token_embeddings[:,0::2,:] = rtg_embeddings # really just [:,0,:]
            token_embeddings[:,1::2,:] = state_embeddings # really just [:,1,:]
        
        elif actions is not None and self.model_type == 'naive':
            assert False
        elif actions is None and self.model_type == 'naive': # only happens at very first timestep of evaluation
            assert False
        else:
            raise NotImplementedError()

        # (batch, 1 + 3*context, n_Embedding)
        x = self.drop(token_embeddings)
        x = self.blocks(x)
        x = self.ln_f(x)
        # (batch, 1 + 3*context, vocab_size)
        logits = self.head(x)
        
        if actions is not None and self.model_type == 'reward_conditioned':
            # => my changed it to 1::3 from 2::3
            logits = logits[:, 1::3, :] # only keep predictions from state_embeddings #(batch, seq, emb)
            # (batch, context_length, vocab_size)
        elif actions is None and self.model_type == 'reward_conditioned':
            print("ENTERED HERE ASSERTION ERROR!-------------------------------------------------------------------------------------")
            logits = logits[:, 1:, :]
            
        elif actions is not None and self.model_type == 'naive':
            print("ENTERED HERE ASSERTION ERROR!-------------------------------------------------------------------------------------")
            logits = logits[:, ::2, :] # only keep predictions from state_embeddings
        elif actions is None and self.model_type == 'naive':
            print("ENTERED HERE ASSERTION ERROR!-------------------------------------------------------------------------------------")
            logits = logits # for completeness
        else:
            raise NotImplementedError()
        
        
        # Passing only the obss through the action_head_s
        # action_h = self.action_head_s(states[:, :, :].reshape(-1, 3, grid, grid)[:, 1, :, :].reshape(-1, 1, grid, grid))
        if machine == 0:
            action_h = self.action_head_s(states[:, :, :].reshape(-1, 3+nFeatures, grid, grid)[:, 1, :, :].reshape(-1, 1, grid, grid))
        else:
            action_h = self.action_head_s(states[:, :, :].reshape(-1, 3+nFeatures, 8, 10)[:, 1, :, :].reshape(-1, 1, 8, 10))
        # => f (my) July 8
        # action_h = self.action_head_s(states[:, :, :].reshape(-1, 8, grid, grid)[:, 1, :, :].reshape(-1, 1, grid, grid))
        # => f (my)
        # action_h = self.action_head_s(states[:, :, :].reshape(-1, 26, grid, grid)[:, 1, :, :].reshape(-1, 1, grid, grid))
        #=> hw my
        # action_h = self.action_head_s(states[:, :, :].reshape(-1, 2, grid, grid)[:, 1, :, :].reshape(-1, 1, grid, grid))
        if machine == 0:
            action_h = action_h.reshape(states.shape[0] * states.shape[1], 1, grid, grid)
        else:
            action_h = action_h.reshape(states.shape[0] * states.shape[1], 1, 8, 10)
        
        
        # (batch*context, 1, grid, grid)
        # (batch*context, 1, grid, grid) => vocab_length = grid ^2
        if machine == 0:
            logits_action_h = torch.cat((logits.reshape(states.shape[0] * states.shape[1], 1, grid, grid), action_h), dim=1)
        else:
            logits_action_h = torch.cat((logits.reshape(states.shape[0] * states.shape[1], 1, 8, 10), action_h), dim=1)
        # (batch*context, grid*grid = vocab_length)
        logits = self.one_kernel(logits_action_h)
        
        # (batch, context, grid*grid = vocab_length) => ======================= This is the stufff we are 
        # -------------------------------------------------------------------------------------
        logits = logits.reshape(states.shape[0], states.shape[1], -1)
        
        # TODO: Double check if the mask indexing is correct or not!
        if machine == 0:
            mask = states.reshape(-1, 3+nFeatures, grid, grid)[:, 3+nFeatures-1].reshape(states.shape[0], states.shape[1], grid * grid)  # The 3rd one obss_mask is used here raw
        else:
            mask = states.reshape(-1, 3+nFeatures, 8, 10)[:, 3+nFeatures-1].reshape(states.shape[0], states.shape[1], 8 * 10)  # The 3rd one obss_mask is used here raw
        #=> previouslly. 8 (f)
        #=> f my
        # mask = states.reshape(-1, 26, grid, grid)[:, 7].reshape(states.shape[0], states.shape[1], grid * grid)  # The 3rd one obss_mask is used here raw
        #=> hw my
        # mask = states.reshape(-1, 2, grid, grid)[:, 1].reshape(states.shape[0], states.shape[1], grid * grid)  # The 3rd one obss_mask is used here raw
        # print(mask, logits)
        # zz = input()
        logits = logits - 1.0e8 * mask        
        # print(logits.shape)
        
        
        
        # if we are given some desired targets also calculate the loss
        loss = None
        
        if targets is not None:
            lengths = lengths.reshape(states.shape[0], states.shape[1] , 1)  # (batch, context, 1)
            targets_tmp = torch.where(lengths == 1, targets, -1)  # (batch, context, 1)
            

            loss = F.cross_entropy(logits.reshape(-1, logits.size(-1)), targets_tmp.reshape(-1), ignore_index = -1)

            _, res = torch.max(logits.reshape(-1, logits.size(-1)), dim=1)
            # my=>
            # tem = res.view(-1, 100)
            # print(tem)
            # print(res[-100:])
            
            real_test_num = lengths.sum()
            acc = ((res == targets_tmp.reshape(-1)).float().sum()) / real_test_num 
            if random.random()<=0.001:
                print("testing returns...")
                test_res = res.reshape(-1, targets.shape[1])[0].cpu()
                print("test_res", test_res)
                print("test_res shape", test_res.shape)
                test_targets = targets.squeeze()[0]
                print("test_targets", test_targets)
                print("test_targets shape", test_targets.shape)
                benchmark = benchmark_id_to_name[benchmarks[0, 0].item()]
                print("benchmarks", benchmarks[0].squeeze())
                test_returns(test_res.numpy(), test_targets.cpu().numpy(), benchmark = benchmark)
        else:
            acc = None

        return logits, loss, acc
