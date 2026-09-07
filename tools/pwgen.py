import bcrypt
import sys

if len(sys.argv) > 2:
    print("not enough arguments")
    sys.exit(1)
    
print(bcrypt.hashpw(sys.argv[1].encode(), bcrypt.gensalt()).decode())