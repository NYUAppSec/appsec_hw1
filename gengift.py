import struct
import sys

# We build the content of the file in a byte string first
# This lets us calculate the length for the header at the end
data = b''
data += b"GiftCardz.com".ljust(32, b' ') # Merchant ID
data += b"B"*32 # Customer ID
data += struct.pack("<I", 1) # One record
# Record of type message
data += struct.pack("<I", 8 + 32)       # Record size: 4 bytes size, 4 bytes type, 32 bytes message
data += struct.pack("<I", 2)            # Record type
data += b"x"*31 + b'\0'                 # Note: 32 byte message

f = open(sys.argv[1], 'wb')
datalen = len(data) + 4 # Plus 4 bytes for the length itself
f.write(struct.pack("<I", datalen))
f.write(data)
f.close()
