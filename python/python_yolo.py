import os
import shutil
import sys
import cv2
import datetime 
import time

from ultralytics import YOLO
#import torch

def classifyImageToFolder(foldername):
    yolo8_person = os.path.join (os.getcwd(), 'yolo8_person')
    yolo8_nonperson = os.path.join (os.getcwd(), 'yolo8_nonperson')

    # Load a model
    #device = torch.device('cuda')
    model = YOLO("./yolov8n.pt")  # load a pretrained model (recommended for training)
    #model.to(device)
    
    file_count = 0
    person_count = 0
    nonperson_count = 0
    data_set = []

    
    if not os.path.isdir(yolo8_person):
        os.mkdir(yolo8_person)
    
    if not os.path.isdir(yolo8_nonperson):
        os.mkdir(yolo8_nonperson)
    

    t1 = time.time()
    for filename in os.listdir(foldername):
        t11 = time.time()
        isperson = 0
        file_count += 1
        #print (os.path.join(foldername ,filename))

        srcimage = os.path.join(foldername ,filename)
        img = cv2.imread(srcimage)

        # Ultralytics yolov8 to detect person!
        results = model(img)
        if len(results) == 0:
            nonperson_count += 1
            continue

        for cc in results[0].boxes.cls:
            if cc == 0: # 0 is person
                isperson = 1
                break

        if isperson == 1:
            person_count += 1
            #results[0].save(filename=os.path.join(yolo8_person, filename))
            #shutil.copyfile(srcimage, os.path.join(yolo8_person, filename))
            #data_set.append(1)
        else:
            nonperson_count += 1
            #shutil.copyfile(srcimage, os.path.join(yolo8_nonperson, filename))
            #data_set.append(0)

        t2 = time.time()
        #data_set.append(t2-t11)
        print ('total file: %d, person:%d(%.3f), nonperson: %d(%.3f), spend time:%.3f (total: %.2f)'% (file_count, person_count, person_count/file_count, nonperson_count, nonperson_count/file_count, t2-t11, t2-t1))
        #time.sleep(10)
    print ('======\nUltralytics yolov8 Target folder: ' + foldername)

    
    print ('data_set data: [')
    for tt in data_set:
        #print ('%.4f, '%tt, end='')
        print ('%d, '%tt, end='')
    print (']')
    


def main():
    if len(sys.argv) < 2:
        print ('Please assign image source forder like D:\\ImageNet')
        return
    else:
        foldername = sys.argv[1]

    if os.path.isdir(foldername):
        print ('foldername: [%s]' % foldername)
    else:
        print ('foldername: [' + foldername + '] is not a exist folder!')
        return

    classifyImageToFolder(foldername)

if __name__ == "__main__":
    main()
    