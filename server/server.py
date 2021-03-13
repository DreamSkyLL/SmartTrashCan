# -*- coding: utf-8 -*-

import os
import socket
from threading import Thread
from aip import AipImageClassify
import requests, json

IP_PORT=("192.168.31.238", 51888)
# IP_PORT=("192.168.43.103", 51888)
# IP_PORT=("192.168.182.1", 51888)
# IP_PORT=("192.168.2.38", 51888)
BUFF_SIZE=1024

APP_ID = ''
API_KEY = ''
SECRET_KEY = ''

def get_file_content(filePath):
    with open(filePath, 'rb') as fp:
        return fp.read()


def get_kind(name):
    r = requests.get('http://api.tianapi.com/txapi/lajifenlei/index', params={
        'key': '75da825756ea656c0dc629a3dfeca8cf',
        'word': name,
    })
    data = json.loads(r.text)
    try:
        kind = data['newslist'][0]['type']
    except:
        return 4
    return kind


def recv_picture(conn, addr):
    client = AipImageClassify(APP_ID, API_KEY, SECRET_KEY)
    with open('pic.jpeg', 'wb') as fp:
        first=conn.recv(BUFF_SIZE)
        file_size, first = first.split(bytes('<SEPARATOR>', 'utf-8'))
        file_size = int(file_size)
        print(file_size)
        fp.write(first)
        file_size -= len(first)
        while file_size > 0:
            data = conn.recv(BUFF_SIZE)
            if not data:
                break
            fp.write(data)
            file_size -= len(data)
        print('picture received')
        fp.close()
    img = get_file_content("pic.jpeg")
    res = client.advancedGeneral(img)
    name = res['result'][0]['keyword']
    kind = get_kind(name)
    kind_list = ['可回收垃圾', '有害垃圾', '厨余垃圾', '其他垃圾', '无法识别']
    kind_name = kind_list[int(kind)]
    print('类型：%s' % (kind_name))
    # res=recognize.application()
    conn.send(bytes(str(kind)+'\r', 'utf-8'))
    conn.close()


def server():
    sk = socket.socket()
    sk.bind(IP_PORT)
    sk.listen(5)
    sk.setblocking(True)
    sk.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    while True:
        conn, addr = sk.accept()
        print(f"{addr}客户端已经连接")
        p = Thread(target=recv_picture, args=(conn, addr))
        p.start()


if __name__=='__main__':
    server()
