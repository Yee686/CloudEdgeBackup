import psutil
import time
import csv
import sys
import select

start_time = time.time()
with open('/root/cloudbackup/CloudEdgeBackup/my_test/system_usage_recovery.csv', mode='w', newline='') as file:
    writer = csv.writer(file)
    writer.writerow(['Time', 'CPU Usage (%)', 'Memory Usage (%)', 'Network Usage (MB)'])
    
    count = 1
    
    while True:
        current_time = time.strftime('%H:%M:%S', time.localtime())
        cpu_percent = psutil.cpu_percent()
        mem_percent = psutil.virtual_memory().percent
        net_bytes = psutil.net_io_counters().bytes_sent / (1024*1024)

        writer.writerow([current_time, cpu_percent, mem_percent, net_bytes])
        file.flush()

        print("{}:time[{}], cpu[{}], mem[{}], net[{}]".format(count, current_time, cpu_percent, mem_percent, net_bytes))
        
        count += 1
        # time.sleep(0.2)

        # 使用 select 模块检测标准输入是否有数据可读，超时参数设置为 0.1 秒
        if sys.stdin in select.select([sys.stdin], [], [], 0.1)[0]:
            line = input()
            if line == 'e' or line == 'E':
                end_time = time.time()
                run_time = end_time - start_time
                print("程序运行时间：", run_time, "秒")

                break