for i in range(1, 11):
    file = open(f"/backup/test{i}", "w")

    for j in range(1, i+1):
        file.write(f"\n")
        for k in range(1, 50):  
            file.write(f"{j} 这是文件{i}的测试内容 \n")
    
    file.close()