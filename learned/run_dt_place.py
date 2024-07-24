import os
import logging
from mingpt.utils import set_seed
import numpy as np
import torch
from torch.utils.data import Dataset
from mingpt.model_placement import GPT, GPTConfig
from mingpt.trainer_placement import Trainer, TrainerConfig
from mingpt.trainer_placement import circuit_feas
import torch
import argparse
import pandas as pd
from create_dataset import create_dataset
from yr_utils import generate_timesteps, generate_timesteps_for_eval, machine
from torch.utils.data.dataloader import DataLoader

def get_parameter_number(model):
    total_num = sum(p.numel() for p in model.parameters())
    trainable_num = sum(p.numel() for p in model.parameters() if p.requires_grad)
    print({'Total': total_num, 'Trainable': trainable_num})

parser = argparse.ArgumentParser()
parser.add_argument('--seed', type=int, default=123)
parser.add_argument('--context_length', type=int, default=100)  # my=> 100 in stead of 256
parser.add_argument('--epochs', type=int, default=10000)
parser.add_argument('--batch_size', type=int, default=32)
parser.add_argument('--cuda', type=str, default='1,2,3')
parser.add_argument('--is_eval_only', action='store_true')
parser.add_argument('--test_all_macro', action='store_true')
parser.add_argument('--start_cfg', type=int, default=30)
parser.add_argument('--rtg', type=float, default=1.1)
args = parser.parse_args()

os.environ['CUDA_VISIBLE_DEVICES'] = args.cuda

set_seed(args.seed)
grid = 8  #=>my

seq_len = args.context_length
rtg_scale = args.rtg
cfg_to_start_with = args.start_cfg

class StateActionReturnDataset(Dataset):

    def __init__(self, data, block_size, actions, done_idxs, rtgs, 
            timesteps, meta_data = None, obss_wire = None, obss_mask = None, benchmarks = None,
            stepwise_returns = None, lengths = None):
        assert block_size % 3 == 0
    
        self.block_size = block_size
        self.seq_len = self.block_size // 3
        if machine == 0:
            self.vocab_size = int((grid) ** 2)
        else: 
            self.vocab_size = 8 * 10
        self.data = data
        self.actions = actions
        print("data raw shape", data.shape)
        self.done_idxs = done_idxs
        self.meta_data = meta_data
        # print("meta_data raw shape", meta_data.shape)
        self.rtgs = rtgs
        self.timesteps = timesteps
        self.obss_wire = obss_wire
        self.obss_mask = obss_mask
        self.benchmarks = benchmarks
        self.stepwise_returns = stepwise_returns
        self.lengths = lengths
    
    def __len__(self):
        return len(self.data)//self.seq_len

    def __getitem__(self, idx):
        block_size = self.block_size // 3
        idx = idx * self.seq_len
        done_idx = idx + self.seq_len
        if self.obss_mask is None:
            states = torch.tensor(np.array(self.data[idx:done_idx]), 
                dtype=torch.float32).reshape(block_size, -1) # (block_size, 4*84*84)
        else:
            tmp_obss = torch.tensor(np.array(self.data[idx:done_idx]), 
                dtype=torch.float32).reshape(block_size, -1)
            tmp_obss_wire = torch.tensor(np.array(self.obss_wire[idx:done_idx]), 
                dtype=torch.float32).reshape(block_size, -1)
            tmp_obss_mask = torch.tensor(np.array(self.obss_mask[idx:done_idx]), 
                dtype=torch.float32).reshape(block_size, -1)
            
            states = torch.cat((tmp_obss, tmp_obss_wire, tmp_obss_mask), dim=1)
            # => h/w my change for hw
            # states = torch.cat((tmp_obss, tmp_obss_mask), dim=1)

        meta_states = torch.tensor(np.array(self.meta_data[idx:done_idx]), dtype=torch.float32).reshape(block_size, -1)
        actions = torch.tensor(self.actions[idx:done_idx], dtype=torch.long).unsqueeze(1) # (block_size, 1)
        
        rtgs = torch.tensor(self.rtgs[idx:done_idx], dtype=torch.float32).unsqueeze(1)
        timesteps = torch.tensor(self.timesteps[idx:done_idx], dtype=torch.int64).unsqueeze(1)
        benchmarks = torch.tensor(self.benchmarks[idx:done_idx], dtype=torch.int64).unsqueeze(1)
        stepwise_returns = torch.tensor(self.stepwise_returns[idx:done_idx], dtype=torch.float32).unsqueeze(1)
        benchmark_id = int(self.benchmarks[idx][0])
        circuit_feas_for_benchmark = torch.tensor(circuit_feas[benchmark_id], dtype = torch.float32)
        length = torch.zeros((block_size,), dtype=torch.bool)
        length[:int(self.lengths[idx][0])] = 1
        return states, actions, rtgs, timesteps, meta_states, \
            benchmarks, stepwise_returns, circuit_feas_for_benchmark, length


obss, obss_s, obss_mask, actions, stepwise_returns, rtgs, done_idxs, timesteps, meta_data, lengths, benchmarks = generate_timesteps(True, rtg_scale)
# DF = pd.DataFrame(np.reshape(obss_s, (obss_s.shape[0], -1))[:1000]) 
# DF.to_csv("data1.csv")


# => my 
obss_, obss_s_, obss_mask_, actions_, stepwise_returns_, rtgs_, done_idxs_, timesteps_, meta_data_, lengths_, benchmarks_ = generate_timesteps_for_eval(cfg_to_start_with, True, rtg_scale)

print("============================================================================================================")
print("create dataset finish.")
print("obss shape = ", obss.shape)  # (records, 1, grid, grid) => False, true
print("obss_wire shape = ", obss_s.shape)  # (records, 1, grid, grid)  => float
print("obss_mask shape = ", obss_mask.shape)  # (records, 1, grid, grid)  => True, false

print("actions shape = ", actions.shape)  # (records, ) => int
# print("returns shape = ", returns.shape)  # (101, 1) => float
print("done_idxs shape = ", done_idxs.shape)  # (100, ) => 256 * i => 256, 512, 768
print("rtgs shape = ", rtgs.shape)  # (records, )  => float

print("timesteps shape = ", timesteps.shape)  # (records, )  => [0-255][0-255][0-255]
print("meta_data shape = ", meta_data.shape)  # (records, 6)  => negative values

print("benchmarks shape = ", benchmarks.shape)  # (records, 1)  => all 0s`
print("stepwise_returns shape = ", stepwise_returns.shape)  # (records, 1)  => float
print("lengths shape = ", lengths.shape)  # (records, 1) => 63s and 0s
print("============================================================================================================")

# load the pickle file and build the dataset
# obss, actions, returns, done_idxs, rtgs, \
#     timesteps, meta_data, obss_wire, obss_mask, benchmarks, \
#     stepwise_returns, lengths = \
#     create_dataset(0, 0, 0, 
#     0, 0, args.is_eval_only)


print("create dataset finish.")

# print("obss shape", len(obss), len(obss[0]))
# print("actions", actions)
# print("actions shape", len(actions))
# print("returns shape", len(returns))
# print("done_idxs shape", len(done_idxs))
# print("rtgs", rtgs)
# print("rtgs shape", len(rtgs))
# print("============================================================================================================")
# print("create dataset finish.")
# print("obss shape = ", obss.shape)  # (records, 1, grid, grid) => False, true
# print("obss_wire shape = ", obss_wire.shape)  # (records, 1, grid, grid)  => float
# print("obss_mask shape = ", obss_mask.shape)  # (records, 1, grid, grid)  => True, false

# print("actions shape = ", actions.shape)  # (records, ) => int
# print("returns shape = ", returns.shape)  # (101, 1) => float
# print("done_idxs shape = ", done_idxs.shape)  # (100, ) => 256 * i => 256, 512, 768
# print("rtgs shape = ", rtgs.shape)  # (records, )  => float

# print("timesteps shape = ", timesteps.shape)  # (records, )  => [0-255][0-255][0-255]
# print("meta_data shape = ", meta_data.shape)  # (records, 6)  => negative values

# print("benchmarks shape = ", benchmarks.shape)  # (records, 1)  => all 0s`
# print("stepwise_returns shape = ", stepwise_returns.shape)  # (records, 1)  => float
# print("lengths shape = ", lengths.shape)  # (records, 1) => 63s and 0s
# print("============================================================================================================")


# set up logging
logging.basicConfig(
        format="%(asctime)s - %(levelname)s - %(name)s -   %(message)s",
        datefmt="%m/%d/%Y %H:%M:%S",
        level=logging.INFO,
)
# my=>
train_dataset = StateActionReturnDataset(obss, args.context_length*3, actions, 
    done_idxs, rtgs, timesteps, meta_data, obss_s, 
    obss_mask, benchmarks, stepwise_returns, lengths)
test_dataset = StateActionReturnDataset(obss_, args.context_length*3, actions_, 
    done_idxs_, rtgs_, timesteps_, meta_data_, obss_s_, 
    obss_mask_, benchmarks_, stepwise_returns_, lengths_)

# To check if loading is done correctly
# loader = DataLoader(train_dataset, shuffle=True, pin_memory=True,
#                                 batch_size=32)
# pbar = enumerate(loader)
# for it, (x, y, r, t, m_x, b, st, cir, l) in pbar:
#     # states, actions, rtgs, timesteps, meta_states, benchmarks, stepwise_returns, circuit_feas_for_benchmark, length
    
#     # place data on the correct device
#     x = x  # my=> (batch, context, 8*grid*grid)
#     m_x = m_x  # my=> (batch, context, 6)
#     y = y  # my=> (batch, context, 1)
#     r = r  # my=> (batch, context, 1, 1) should be (batch, context, 1)
#     t = t  # my=> (batch, context, 1)
#     b = b  # my=> (batch, context, 1, 1)
#     st = st  # my=> (batch, context, 1, 1)
#     cir = cir  # my=> (batch, 768)
#     l = l # my=> (batch, context)
#     print(x.shape, y.shape, r.shape, t.shape, m_x.shape, b.shape, st.shape, cir.shape, l.shape)
#     print(x[0, 5, :].view(-1, ))
#     print(r[0].view(-1, ))
#     zz = input()

# print("!!!! max(timesteps)", max(timesteps))


# This is the place where you can tune stuff => my
mconf = GPTConfig(train_dataset.vocab_size, train_dataset.block_size,
                  n_layer=6, n_head=8, n_embd=128, 
                  model_type="reward_conditioned", 
                  max_timestep=max(timesteps))
model = GPT(mconf)
# July 13 Models
# Runs OSM (EXP-1), EXP2, EXP3
# model_path = "save_models/m_e_1-2-3.pkl" # With 6 features 
# model_path = "save_models/2024-07-17-18-59-59-0.918.pkl"  # this one is one the full dataset 


# For Quad Tree
# model_path = "save_models_quad/2024-07-17-03-17-48-0.859.pkl" # With 6 features
# model_path = "save_models_quad/m_e1_quad.pkl" # With 6 features


# For icelake
# model_path = "save_models_quad/icelake_40.pkl.pkl" # With 6 features
# model_path = "save_models_quad/2024-07-17-12-53-43-0.888.pkl" # With 6 features experiment skew

# this is for exp machin:
model_path = "save_models_icelake_quad/2024-07-18-07-36-51-0.909.pkl" # With 6 features experiment skew

# model_path = None
if model_path is not None:
    state_dict = torch.load(model_path, map_location=torch.device('cpu'))
    for k,v in state_dict.items():
        if "module." in k:
            state_dict[k.split('.', 1)[1]] = v
        else:
            state_dict[k] = v
    model.load_state_dict(state_dict, strict = True)
model.eval()
get_parameter_number(model)

# initialize a trainer instance and kick off training
epochs = args.epochs
# => Changed here
args.is_eval_only = True

tconf = TrainerConfig(max_epochs=epochs, batch_size=args.batch_size, learning_rate=6e-4,
                      lr_decay=True, warmup_tokens=512*20, final_tokens=2*len(train_dataset)*args.context_length*3,
                      num_workers=1, seed=args.seed, model_type="reward_conditioned", max_timestep=max(timesteps),
                      draw_placement = True, is_eval_only = args.is_eval_only,
                      test_all_macro = args.test_all_macro)
print("trainerconfig finish")
# => my test_dataset in place of None
trainer = Trainer(model, train_dataset, test_dataset, tconf, cfg_to_start_with)
print("trainer build finish")
trainer.train()