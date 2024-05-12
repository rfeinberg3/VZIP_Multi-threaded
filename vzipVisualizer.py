import cv2
import zlib
import numpy as np
import sys

def read_vzip(filename):
    with open(filename, 'rb') as file:
        while True:
            # Read the size of the next compressed frame
            size_data = file.read(4)
            if not size_data:
                break  # End of file reached

            # Convert the size to an integer
            nbytes_zipped = int.from_bytes(size_data, byteorder='little', signed=False)

            # Read the compressed frame data
            buffer_out = file.read(nbytes_zipped)

            # Decompress the frame data
            buffer_in = zlib.decompress(buffer_out)

            # Find the end of the PPM header (after "\n255\n")
            header_end_index = buffer_in.find(b'\n255\n') + len(b'\n255\n')

            # Extract the image data (skipping the header)
            image_data = buffer_in[header_end_index:]

            # Reshape the image data to the correct dimensions
            frame = np.frombuffer(image_data, dtype=np.uint8).reshape((720, 406, 3))

            yield frame

def play_video(frames):
    for frame in frames:
        # Display the frame
        cv2.imshow('Frame', frame)

        # Break the loop and close the window if 'q' is pressed
        if cv2.waitKey(25) & 0xFF == ord('q'):
            break

    # Clean up: close all OpenCV windows
    cv2.destroyAllWindows()

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python visualizer.py <path_to_vzip_file>")
        sys.exit(1)

    vzip_file = sys.argv[1]  # Command-line argument for the vzip file path

    frames = read_vzip(vzip_file)
    play_video(frames)