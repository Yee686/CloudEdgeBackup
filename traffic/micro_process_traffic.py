import psutil
import time
import matplotlib.pyplot as plt
import sys
import subprocess
from datetime import datetime as dt



def get_process_pid(process_name):
    try:
        while True:
            ps_output = subprocess.check_output(['ps', '-e', '-o', 'pid,command'])
            # 将输出按行分割
            ps_lines = ps_output.decode().strip().split('\n')

            for line in ps_lines[1:]:
                parts = line.split(None, 1)
                pid = int(parts[0])
                process_command = parts[1]

                # 检查命令是否匹配
                if command in process_command:
                    return pid
        # 如果没有找到匹配的进程，返回 None
        return -1

    except subprocess.CalledProcessError:
        return None

def get_process_traffic(pid, date):
    
    total_bytes_sent = 0
    total_bytes_received = 0

    interval_bytes_sent = 0
    interval_bytes_received = 0

    setup_time = time.time()
    
    io_counter = psutil.net_io_counters()
    recvBytes = io_counter.bytes_recv
    sentBytes = io_counter.bytes_sent
    
    process = psutil.Process(pid)
    with open(f'/sync_worker/traffic/log/micro/process_traffic_{date}.txt', 'w') as file:
        while True:
            start_time = time.time()
            time.sleep(1)

            
            if not process.is_running():
                file.close()
                break
            
            try:
                io_counter = psutil.net_io_counters()
                recvBytes_pre = recvBytes
                sendBytes_pre = sentBytes

                recvBytes = io_counter.bytes_recv
                sentBytes = io_counter.bytes_sent

                interval_bytes_sent = sentBytes - sendBytes_pre
                interval_bytes_received = recvBytes - recvBytes_pre
            except psutil.NoSuchProcess:
                # 处理连接已关闭但仍在进程连接列表中的情况
                pass

            total_bytes_sent = sentBytes
            total_bytes_received = recvBytes

            end_time = time.time()
            interval = end_time - start_time

            start_tt = start_time - setup_time
            end_tt = end_time - setup_time

            output = "[ {:.1f} - {:.1f} sec] sent {:.2f} MB, up_bandwidth {:.4f} MB/s, recv {:.2f} MB, down_bandwidth {:.4f} MB/s".format(
                start_tt, end_tt, interval_bytes_sent / 1024 / 1024, interval_bytes_sent / interval / 1024 / 1024,
                interval_bytes_received / 1024 / 1024, interval_bytes_received / 1024 / 1024)

            print(output)
            file.write(output + '\n')
            file.flush()
        

    return total_bytes_sent, total_bytes_received

def plot_traffic(date):
    with open(f'/sync_worker/traffic/log/micro/process_traffic_{date}.txt', 'r') as file:
        lines = file.readlines()
        time_list = []
        up_list = []
        down_list = []
        for line in lines:
            items = line.strip().split()
            time_list.append(int(eval(items[3])))
            up_list.append(eval(items[9]))
            down_list.append(eval(items[15]))
        plt.plot(time_list, down_list, label='Down')
        plt.plot(time_list, up_list, label='Up')
        plt.xlabel('Time(s)')
        plt.ylabel('Bandwidth(MB/s)')
        plt.ylim(0, 2)
        plt.title("Backup Bandwidth Plot")
        plt.legend()
        plt.savefig(f"/sync_worker/traffic/plot/micro/process_traffic_{date}.png")

if __name__ == '__main__':
    command = "python3 micro_backup.py"
    pid = get_process_pid(command)
    date = dt.now().strftime("%Y-%m-%d-%H:%M:%S")

    if pid == None:
        print("Error executing ps")
        sys.exit(1)
    elif pid == -1:
        print("Process not found")
        sys.exit(1)
    else:
        print("Process found, pid: %d" % pid)
        get_process_traffic(pid, date)

    plot_traffic(date)
