import random
import string
import os

def generate_random_string(length):
    """
    生成指定长度的随机字符串
    """
    return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

def generate_random_file(file_path, file_size):
    """
    生成指定大小的随机内容文件
    """
    with open(file_path, 'w') as f:
        for i in range(file_size):
            for j in range(0,16):
                f.write(generate_random_string(64) + '\n')

# 测试参数
n = 20  # 文件数目
m = 1  # 文件大小（行数）1行1k

# 生成随机文件
for k in range(1,17):
    print(f"创建{n}个大小为{m}k的文件...")
    folder_path = "/home/7948lkj/CloudEdgeBackup/CloudEdgeBackup/my_test/file_size_test/"+str(m)+"k"
    if not os.path.exists(folder_path):
            os.mkdir(folder_path)
    
    print(f"存放于{folder_path}")
    for i in range(n):
        file_path = f'{folder_path}/file_{i}.txt'
        generate_random_file(file_path, m)
    m = m * 2

    print(f"创建成功\n")