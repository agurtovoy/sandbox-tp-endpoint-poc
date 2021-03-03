import json
import numpy
import sys
import os


def json_to_list(input_file):
    with open(input_file, "r") as input:
        return json.load(input)

def npy_to_list(input_file):
    return numpy.load(input_file).tolist()

def binary_to_list(input_file):
    return numpy.fromfile(input_file).tolist()


def list_to_json(list, output_file):
    with open(output_file, "w") as output:
        json.dump(list, output, indent=4)

def list_to_npy(list, output_file):
    numpy.save(output_file, numpy.array(list, dtype=numpy.float32))

def list_to_binary(list, output_file):
    with open(output_file, "wb") as out:
        out.write(numpy.array(list).tobytes())

input_convertor = {
    ".json": json_to_list,
    ".npy": npy_to_list,
    ".bin": binary_to_list
}

output_convertor = {
    ".json": list_to_json,
    ".npy": list_to_npy,
    ".bin": list_to_binary
}

def file_ext(path): return os.path.splitext(path)[1]

def main(input_file, output_file):
    input_func = input_convertor[file_ext(input_file)]
    output_func = output_convertor[file_ext(output_file)]
    output_func(input_func(input_file), output_file)

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
