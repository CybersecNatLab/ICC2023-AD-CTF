#!/usr/bin/env python3

import requests
import sys


def main():
    if len(sys.argv) != 5:
        print(f'Usage: {sys.argv[0]} <API URL> <username> <script file> <input file>', file=sys.stderr)
        exit(1)

    api_url, username, script_path, input_path = sys.argv[1:]

    with open(script_path, 'rb') as f:
        script_content = f.read()
    with open(input_path, 'rb') as f:
        input_content = f.read()

    data = ';'.join(map(bytes.hex, [username.encode(), script_content, input_content]))
    r = requests.post(api_url, data=data.encode())
    print(f'Status: {r.status_code} {r.reason}')
    print(r.content)


if __name__ == '__main__':
    main()
