file_name = '/home/kenny/data/Rfam.seed'
counter = 0
with open(file_name, 'rb') as f:
    for line in f:
        if(line == b'# STOCKHOLM 1.0\n'):
            if(counter !=0):
                tempFile.close()
            counter += 1
            tempFile = open('/home/kenny/data/msa/RF000{}.msa'.format(counter), "xb")


        tempFile.write(line)


        #print(line) if counter < 1000 else 0

