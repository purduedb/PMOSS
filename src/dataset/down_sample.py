import numpy as np
import struct
import os

def downsample(fn):
    # if os.path.exists("/scratch1/yrayhan/" + fn + "_600M_uint64"):
    #     return

    print("Downsampling", fn)
    d = np.fromfile("/users/yrayhan/works/erebus/src/dataset/" + fn + "_800M_uint64", dtype=np.uint64)[1:]
    nd = np.delete(d, np.arange(0, d.size, 4))

    # with open("/scratch1/yrayhan/" + fn + "_600M_uint64", "wb") as f:
    #     f.write(struct.pack("Q", len(nd)))
    #     nd.tofile(f)

    # nd = d[::2]
    # with open("/scratch1/yrayhan/" + fn + "_400M_uint64", "wb") as f:
    #     f.write(struct.pack("Q", len(nd)))
    #     nd.tofile(f)

    # nd = d[::4]
    # with open("/scratch1/yrayhan/" + fn + "_200M_uint64", "wb") as f:
    #     f.write(struct.pack("Q", len(nd)))
    #     nd.tofile(f)
    
    nd = d[::8]
    with open("/users/yrayhan/works/erebus/src/dataset/" + fn + "_100M_uint64", "wb") as f:
        f.write(struct.pack("Q", len(nd)))
        nd.tofile(f)
    
    

# downsample("books")
downsample("osm_cellids")