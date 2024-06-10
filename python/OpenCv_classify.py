import os
import shutil
import sys
import cv2
import datetime 
import time

def classifyImageToFolder(foldername):
    #CvPersonScore0 = os.path.join (os.getcwd(), 'CvPersonScore0')
    #CvPersonScore2 = os.path.join (os.getcwd(), 'CvPersonScore2')
    face_cascade = cv2.CascadeClassifier("haarcascade_frontalface_default.xml")
    ubody_cascade = cv2.CascadeClassifier("haarcascade_upperbody.xml")
    face_count = 0
    ubody_count = 0
    either_count = 0
    both_count = 0
    file_count = 0
    none_count = 0

    '''
    if not os.path.isdir(CvPersonScore0):
        os.mkdir(CvPersonScore0)

    if not os.path.isdir(CvPersonScore2):
        os.mkdir(CvPersonScore2)
    '''

    t1 = time.time()
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
            face_count += 1

        # detect upper body
        ubodys = ubody_cascade.detectMultiScale(gray)
        if len(ubodys):
            detectUbody = True
            ubody_count += 1
            
        if detectFace and detectUbody:
            both_count += 1
            #cv2.imwrite(os.path.join(CvPersonScore2, filename), img)
            #shutil.copyfile(srcimage, os.path.join(CvPersonScore2, filename))
        
        if not detectFace and not detectUbody:
            none_count += 1
            #cv2.imwrite(os.path.join(CvPersonScore0, filename), img)
            #shutil.copyfile(srcimage, os.path.join(CvPersonScore0, filename))
        
        if detectFace or detectUbody:
            either_count += 1


        t2 = time.time()
        print ('total file: %d, face:%d(%.3f), ubody: %d(%.3f), both: %d(%.3f), either: %d(%.3f), none: %d(%.3f), spend time %.2f'% (file_count, face_count, face_count/file_count, ubody_count, ubody_count/file_count, both_count, both_count/file_count, either_count, either_count/file_count, none_count, none_count/file_count, t2-t1))
    print ('======\nTarget folder: ' + foldername)


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
    