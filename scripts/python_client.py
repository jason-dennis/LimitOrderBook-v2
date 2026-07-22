import socket
import struct
import random
import time

def encode_order(price, quantity, type, side, tif, trader_id, symbol):
    scaled_price = int(price * 100)
    symbol_bytes = symbol.encode('ascii')
    symbol_length = len(symbol_bytes)
    header = struct.pack('>BBQIIB',type, side, scaled_price, quantity, trader_id, tif)
    length_byte = struct.pack('>B',symbol_length)
    return header + length_byte + symbol_bytes


def client_program():
    host = "localhost"
    port = 9000
    client_socket = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    client_socket.connect((host,port))

    while True:

        price = random.uniform(99.0,130.0)
        quantity = random.randint(3,10)
        side = random.choice([0,1])
        type = 0
        tif = 0
        trader_id = random.randint(1,1000)
        symbol = "AAPL"

        message = encode_order(price, quantity, type, side, tif, trader_id, symbol)

        client_socket.send(message)
        time.sleep(random.uniform(0.5,2.0))


if __name__ == "__main__":
    try:
        client_program()
    except KeyboardInterrupt:
        print("\nClient oprit manual.")
    except BrokenPipeError:
        print("\nConexiunea cu serverul s-a intrerupt.")