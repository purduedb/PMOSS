import numpy as np

files = [
#   "wiki_ts_200M_uint64", 
# "fb_200M_uint64",
# "osm_cellids_100M_uint64"
"osm_cellids_200M_uint64"
]

# for _ in files:
#   fname = "/users/yrayhan/works/erebus/src/dataset/" + _  
#   pts = np.fromfile(fname, dtype=np.uint64)
#   size = 100000000
#   print(pts.shape)
#   print(min(pts[1:size+2]), max(pts[1:size+2]))
# exit(0)

for _ in files:
  fname = "/users/yrayhan/works/erebus/src/dataset/" + _  
  pts = np.fromfile(fname, dtype=np.uint64)
  nf = open(fname + ".dat", "w")
  for _ in range(1, pts.shape[0]):
    nf.write("INSERT ")
    nf.write(str(pts[_]))
    nf.write("\n")
  nf.close()
  

