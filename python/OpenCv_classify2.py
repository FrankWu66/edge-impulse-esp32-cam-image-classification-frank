import os
import shutil
import sys
import cv2
import datetime 
import time


def classifyImageToFolder(foldername):
    CvPersonScore0 = os.path.join (os.getcwd(), 'CvPersonScore0')
    CvPersonScore2 = os.path.join (os.getcwd(), 'CvPersonScore2')
    face_cascade = cv2.CascadeClassifier("haarcascade_frontalface_default.xml")
    ubody_cascade = cv2.CascadeClassifier("haarcascade_upperbody.xml")
    score0_count = 0
    score2_count = 0
    file_count = 0

    if not os.path.isdir(CvPersonScore0):
        os.mkdir(CvPersonScore0)

    if not os.path.isdir(CvPersonScore2):
        os.mkdir(CvPersonScore2)

    for filename in os.listdir(foldername):
        file_count += 1
        detectFace = False
        detectUbody = False
        #print (os.path.join(foldername ,filename))

        srcimage = os.path.join(foldername ,filename)
        img = cv2.imread(srcimage)
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)  # transfer to gray img

        # detect face
        faces = face_cascade.detectMultiScale(gray)
        if len(faces):
            detectFace = True

        # detect upper body
        ubodys = ubody_cascade.detectMultiScale(gray)
        if len(ubodys):
            detectUbody = True
            
        if detectFace and detectUbody:
            score2_count += 1
            #cv2.imwrite(os.path.join(CvPersonScore2, filename), img)
            shutil.copyfile(srcimage, os.path.join(CvPersonScore2, filename))
        elif not detectFace and not detectUbody:
            score0_count += 1
            #cv2.imwrite(os.path.join(CvPersonScore0, filename), img)
            shutil.copyfile(srcimage, os.path.join(CvPersonScore0, filename))

        #img=cv2.resize(img, (640, 480))
        #cv2.imshow('show in clould', img)
        #key=cv2.waitKey(1)  # wait 1ms for keyboard...
        #cv2.destroyWindow('image')

        print ('total file: %d, score2_count: %d(%.2f), score0_count: %d(%.2f), skip file: %d'% (file_count, score2_count, score2_count/file_count, score0_count, score0_count/file_count, file_count-score2_count-score0_count))
        

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