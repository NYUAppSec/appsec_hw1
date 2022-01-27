import struct
import sys

# We build the content of the file in a byte string first
# This lets us calculate the length for the header at the end
data = b''
data += b"A"*32 # Merchant ID
data += b"B"*32 # Customer ID
data += struct.pack("<I", 1) # One record
# Record of type animation
data += struct.pack("<I", 8 + 32 + 256) # Record size (4 bytes)
data += struct.pack("<I", 3)            # Record type (4 bytes)
data += b"A"*31 + b'\x00'               # Note: 32 byte message
data += b'\x08' * 256                   # Program made entirely of "end program" (256 bytes)

f = open(sys.argv[1], 'wb')
datalen = len(data) + 4 # Plus 4 bytes for the length itself
f.write(struct.pack("<I", datalen))
f.write(data)
f.close()
