import random
import numpy as np
import torch
import torch.nn as nn
from torch.nn import functional as F
torch.set_printoptions(threshold=10000)
from yr_utils import find_correct_max_tput

# grid = 84
# => my
grid = 8
nFeatures = 5 # 5, 23, 10

machine = 1

def set_seed(seed):
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)


def top_k_logits(logits, k):
    v, ix = torch.topk(logits, k)
    out = logits.clone()
    out[out < v[:, [-1]]] = -float('Inf')
    return out


@torch.no_grad()
def sample(model, x, steps, temperature=1.0, sample=False, top_k=None, actions=None, rtgs=None, 
    timesteps=None, meta_state = None, benchmarks = None, stepwise_returns = None, circuit_feas = None,
    is_random_shuffle = False):
    """
    take a conditioning sequence of indices in x (of shape (b,t)) and predict the next token in
    the sequence, feeding the predictions back into the model each time. Clearly the sampling
    has quadratic complexity unlike an RNN that is only linear, and has a finite context window
    of block_size, unlike an RNN that has an infinite context window.
    """
    inp = input()
    print("============================================================================================================================================================")
    # x.shape = [1, 1, 3, 84, 84]
    # None
    # torch.Size([1, 2])
    # torch.Size([1, 1, 1])

    if isinstance(model,torch.nn.DataParallel):
        model = model.module
    block_size = model.get_block_size()
    model.eval()
    print(steps)
    print(x.shape)
    print(actions)
    print(meta_state.shape)
    print(rtgs.shape)
    for k in range(steps):
        print("===========")
        x_cond = x if x.size(1) <= block_size//3 else x[:, -block_size//3:] # crop context if needed
        
        if actions is not None:
            if not torch.is_tensor(actions):
                actions = torch.tensor(actions)
            actions = actions if actions.size(0) <= block_size//3 else actions[:, -block_size//3:] # crop context if needed
        
        if meta_state is not None:
            meta_state_cond = meta_state if meta_state.size(1) <= block_size//3 else meta_state[:, -block_size//3:]
        else:
            meta_state_cond = None
        
        rtgs = rtgs if rtgs.size(1) <= block_size//3 else rtgs[:, -block_size//3:] # crop context if needed
        
        benchmark_id = int(benchmarks[0])
        
        print(x_cond.shape)  # torch.Size([1, 1, 3, 84, 84])
        print(actions)  # 1 
        print(meta_state.shape)  # torch.Size([1, 2])
        print(meta_state_cond.shape) # torch.Size([1, 2])
        print(rtgs.shape)  # torch.Size([1, 1, 1])


        logits, _, _ = model(x_cond, actions=actions, targets=None, rtgs=rtgs, timesteps=timesteps, 
            meta_states = meta_state_cond, benchmarks = benchmarks, circuit_feas= circuit_feas,
            is_random_shuffle = is_random_shuffle)
        
        logits = logits[:, -1, :] / temperature
        print(logits.shape) # torch.Size([1, 7056=grid*grid])

        mask = x_cond.reshape(x_cond.shape[0], x_cond.shape[1], 3, grid, grid)[:, -1, 2].reshape(x_cond.shape[0], grid * grid)
        print(mask.shape)  # torch.Size([1, 7056=grid*grid])

        logits = logits - 1.0e8 * mask
        
        # optionally crop probabilities to only the top k options
        if top_k is not None:
            logits = top_k_logits(logits, top_k)
        temper = 1.0
        probs = F.softmax(logits/temper, dim=-1)
        
        # sample from the distribution or take the most likely
        if sample:
            ix = torch.multinomial(probs, num_samples=1)
        else:
            _, ix = torch.topk(probs, k=1, dim=-1)
        x = ix

    return x, probs


@torch.no_grad()
def sample_a(model, x, steps, temperature=1.0, sample=False, top_k=None, actions=None, rtgs=None, 
    timesteps=None, meta_state = None, benchmarks = None, stepwise_returns = None):
    """
    take a conditioning sequence of indices in x (of shape (b,t)) and predict the next token in
    the sequence, feeding the predictions back into the model each time. Clearly the sampling
    has quadratic complexity unlike an RNN that is only linear, and has a finite context window
    of block_size, unlike an RNN that has an infinite context window.
    """
    if isinstance(model,torch.nn.DataParallel):
        model = model.module
    block_size = model.get_block_size()
    model.eval()
    for k in range(steps):
        x_cond = x if x.size(1) <= block_size//4 else x[:, -block_size//4:] # crop context if needed
        if actions is not None:
            if not torch.is_tensor(actions):
                actions = torch.tensor(actions)
            actions = actions if actions.size(0) <= block_size//4 else actions[:, -block_size//4:] # crop context if needed
        if meta_state is not None:
            meta_state_cond = meta_state if meta_state.size(1) <= block_size//4 else meta_state[:, -block_size//4:]
        else:
            meta_state_cond = None
        rtgs = rtgs if rtgs.size(1) <= block_size//4 else rtgs[:, -block_size//4:] # crop context if needed
        logits, logits_r, _, _, _, _ = model(x_cond, actions=actions, targets=None, rtgs=rtgs, timesteps=timesteps, 
            meta_states = meta_state_cond, benchmarks = benchmarks, stepwise_returns = stepwise_returns)
        logits = logits[:, -1, :] / temperature
        mask = x_cond.reshape(x_cond.shape[0], x_cond.shape[1], 3, grid, grid)[:, -1, 2].reshape(x_cond.shape[0], grid * grid)
        logits = logits - 1.0e8 * mask
        if top_k is not None:
            logits = top_k_logits(logits, top_k)
        # apply softmax to convert to probabilities
        probs = F.softmax(logits, dim=-1)
        # sample from the distribution or take the most likely
        if sample:
            ix = torch.multinomial(probs, num_samples=1)
        else:
            _, ix = torch.topk(probs, k=1, dim=-1)
        x = ix
    return x


@torch.no_grad()
def sample_r(model, x, steps, temperature=1.0, sample=False, top_k=None, actions=None, rtgs=None, 
    timesteps=None, meta_state = None, benchmarks = None, stepwise_returns = None):
    """
    take a conditioning sequence of indices in x (of shape (b,t)) and predict the next token in
    the sequence, feeding the predictions back into the model each time. Clearly the sampling
    has quadratic complexity unlike an RNN that is only linear, and has a finite context window
    of block_size, unlike an RNN that has an infinite context window.
    """
    if isinstance(model,torch.nn.DataParallel):
        model = model.module
    block_size = model.get_block_size()
    model.eval()
    for k in range(steps):
        # 1, block, channel, height, weight
        x_cond = x if x.size(1) <= block_size//4 else x[:, -block_size//4:] # crop context if needed
        if actions is not None:
            if not torch.is_tensor(actions):
                actions = torch.tensor(actions)
            actions = actions if actions.size(0) <= block_size//4 else actions[:, -block_size//4:] # crop context if needed
        if meta_state is not None:
            meta_state_cond = meta_state if meta_state.size(1) <= block_size//4 else meta_state[:, -block_size//4:]
        else:
            meta_state_cond = None
        rtgs = rtgs if rtgs.size(1) <= block_size//4 else rtgs[:, -block_size//4:] # crop context if needed
        logits, logits_r, _, _, _, _ = model(x_cond, actions=actions, targets=None, rtgs=rtgs, timesteps=timesteps, 
            meta_states = meta_state_cond, benchmarks = benchmarks, stepwise_returns = stepwise_returns)
        logits_r = logits_r[:, -1]
        x = logits_r

    return x


@torch.no_grad()
def my_sample(model, x, steps, temperature=1.0, sample=False, top_k=None, actions=None, rtgs=None, 
    timesteps=None, meta_state = None, benchmarks = None, stepwise_returns = None, circuit_feas = None,
    is_random_shuffle = False):
    """
    take a conditioning sequence of indices in x (of shape (b,t)) and predict the next token in
    the sequence, feeding the predictions back into the model each time. Clearly the sampling
    has quadratic complexity unlike an RNN that is only linear, and has a finite context window
    of block_size, unlike an RNN that has an infinite context window.
    """
    print("============================================================================================================================================================")
    # inp = input()
    
    if isinstance(model,torch.nn.DataParallel):
        model = model.module
    
    block_size = model.get_block_size()
    model.eval()


    for k in range(steps):  # you always take a single step 
        print("===========")
        x_cond = x if x.size(1) <= block_size//3 else x[:, -block_size//3:] # crop context if needed
        x_cond =x_cond.to(torch.float32)
        
        print(x_cond.shape)
        
        if actions is not None:
            if not torch.is_tensor(actions):
                actions = torch.tensor(actions)
            actions = actions if actions.size(0) <= block_size//3 else actions[:, -block_size//3:] # crop context if needed
        
        if meta_state is not None:
            meta_state_cond = meta_state if meta_state.size(1) <= block_size//3 else meta_state[:, -block_size//3:]
        else:
            meta_state_cond = None
        
        rtgs = rtgs if rtgs.size(1) <= block_size//3 else rtgs[:, -block_size//3:] # crop context if needed
        
        benchmark_id = int(benchmarks[0])
        
        # print(x_cond.shape)  # torch.Size([1, 1, 3, 84, 84])
        # print(actions)  # 1 
        # print(meta_state.shape)  # torch.Size([1, 2])
        # print(meta_state_cond.shape) # torch.Size([1, 2])
        # print(rtgs.shape)  # torch.Size([1, 1, 1])

        # print(x_cond.dtype, rtgs.dtype, timesteps.dtype, meta_state_cond.dtype)
        # if actions != None: print(actions.dtype)
        # print(x_cond.view(-1, ))
        # print(actions)
        # print(rtgs)
        # print(timesteps)
        # print(meta_state_cond)
        # print(benchmarks)
        
        logits, _, _ = model(x_cond, actions=actions, targets=None, rtgs=rtgs, timesteps=timesteps, 
            meta_states = meta_state_cond, benchmarks = benchmarks, circuit_feas= circuit_feas,
            is_random_shuffle = is_random_shuffle)
        
        logits = logits[:, -1, :] / temperature
        # print(logits.shape) # torch.Size([1, 7056=grid*grid])

        # my => July
        # mask = x_cond.reshape(x_cond.shape[0], x_cond.shape[1], 8, grid, grid)[:, -1, 7].reshape(x_cond.shape[0], grid * grid)
        if machine == 0:
            mask = x_cond.reshape(x_cond.shape[0], x_cond.shape[1], 3+nFeatures, grid, grid)[:, -1, 3+nFeatures-1].reshape(x_cond.shape[0], grid * grid)
        else:
            mask = x_cond.reshape(x_cond.shape[0], x_cond.shape[1], 3+nFeatures, 8, 10)[:, -1, 3+nFeatures-1].reshape(x_cond.shape[0], 8 * 10)
        # print(mask)
        # print(mask.shape)  # torch.Size([1, 7056=grid*grid])
        
        logits = logits - 1.0e8 * mask
        
        # optionally crop probabilities to only the top k options
        if top_k is not None:
            logits = top_k_logits(logits, top_k)
        temper = 1.0
        probs = F.softmax(logits/temper, dim=-1)
        
        # sample from the distribution or take the most likely
        if sample:
            ix = torch.multinomial(probs, num_samples=1)
        else:
            _, ix = torch.topk(probs, k=1, dim=-1)
        x = ix

    return x, probs



