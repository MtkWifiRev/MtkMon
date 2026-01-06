import sys
import os

def pda_crc(buf: bytes) -> int:
    """
    Calculates CRC using the provided algorithm.
    """
    crc = 0
    for pos in range(0, len(buf), 4):
        # Read 4 bytes at a time; if fewer remain, it reads what is available
        v = int.from_bytes(buf[pos:pos+4], 'little')
        
        crc0 = (crc >> 0) & 1
        crc4 = (crc >> 4) & 1
        crc8 = (crc >> 8) & 1
        crc12 = (crc >> 12) & 1
        crc17 = (crc >> 17) & 1
        crc20 = (crc >> 20) & 1
        crc23 = (crc >> 23) & 1
        crc28 = (crc >> 28) & 1
        
        crc_n31 = crc28 ^ crc23 ^ crc20 ^ crc17 ^ crc12 ^ crc8 ^ crc4 ^ crc0
        crc_n = (crc_n31 << 31) | (crc >> 1)
        crc = crc_n ^ v
    return crc

def main():
    # Check for correct number of arguments
    if len(sys.argv) != 3:
        print(f"Usage: python {os.path.basename(sys.argv[0])} <input_binary> <output_binary>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    try:
        # 1. Read the input binary file
        with open(input_file, 'rb') as f_in:
            data = f_in.read()

        # 2. Calculate the CRC
        result_crc = pda_crc(data)
        
        # Display the calculated CRC in Hex for user verification
        print(f"Calculated CRC: 0x{result_crc:08X}")

        # 3. Write the CRC to the output binary file
        # Converting to 4 bytes, little-endian to match the input reading method
        with open(output_file, 'wb') as f_out:
            f_out.write(result_crc.to_bytes(4, byteorder='little'))
            
        print(f"Successfully wrote binary CRC to '{output_file}'")

    except FileNotFoundError:
        print(f"Error: The file '{input_file}' was not found.")
        sys.exit(1)
    except PermissionError:
        print(f"Error: Permission denied when accessing files.")
        sys.exit(1)
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()

