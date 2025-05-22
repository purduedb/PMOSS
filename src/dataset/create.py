import numpy as np

files = [
#   "wiki_ts_200M_uint64", 
# "fb_200M_uint64",
# "osm_cellids_100M_uint64"
# "osm_cellids_200M_uint64"
# "osm_cellids_600M_uint64"
# "loade_zipf_int_100M.dat"
"loade_zipf_int_500M.dat"
]

# Initialize variables to store min and max values
min_value = float('inf')
max_value = float('-inf')
# file_name = "/users/yrayhan/works/erebus/src/dataset/" + files[0] 
# Open and read the file
# with open(file_name, 'r') as file:
#     for line in file:
#         # Split the line by spaces
#         parts = line.split()
#         if len(parts) == 2 and parts[0] == "INSERT":
#             value = int(parts[1])  # Extract the second column value
#             # Update min and max values
#             min_value = min(min_value, value)
#             max_value = max(max_value, value)

# Print the results
# print("Minimum value in the 2nd column:", min_value)
# print("Maximum value in the 2nd column:", max_value)

for _ in files:
  fname = "./src/dataset/" + _  
  pts = np.fromfile(fname, dtype=np.uint64)
  size = 500000000
  print(pts.shape)
  print(pts[0:30])
  print(min(pts[1:size]), max(pts[1:size]))
exit(0)

# for _ in files:
#   fname = "/users/yrayhan/works/erebus/src/dataset/" + _  
#   pts = np.fromfile(fname, dtype=np.uint64)
#   nf = open(fname + ".dat", "w")
#   for _ in range(1, pts.shape[0]):
#     nf.write("INSERT ")
#     nf.write(str(pts[_]))
#     nf.write("\n")
#   nf.close()
  

