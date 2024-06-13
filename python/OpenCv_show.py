import os
import shutil
import sys
import cv2
import datetime 
import time

def show(foldername):
    imgx = 768 #640 #768
    imgy = 576 #480 #576
    for filename in os.listdir(foldername):
        srcimage = os.path.join(foldername ,filename)
        img = cv2.imread(srcimage)
        height, width = img.shape[:2]

        cv2.namedWindow(foldername, cv2.WINDOW_AUTOSIZE)
        if width > height:
            img=cv2.resize(img, (imgx, int(imgx*height/width)))
            cv2.moveWindow(foldername, 0, 0)
        else:
            img=cv2.resize(img, (int(imgy*width/height), imgy))
            cv2.moveWindow(foldername, int((imgx - imgy*width/height)/2), 0)
        
        #cv2.namedWindow(foldername, 0)
        #cv2.resizeWindow(foldername, imgx, imgy)
        cv2.imshow(foldername, img)
        key=cv2.waitKey(500)  # wait ms for keyboard...
   
def main():
    if len(sys.argv) < 2:
        print ('Please assign image source forder like D:\\ImageNet')
        #foldername = os.path.join(os.getcwd(), '..', '..', 'processingImage', 'MIX_PIC')
        foldername = 'E:\processingImage\MIX_PIC'
        #return
    else:
        foldername = sys.argv[1]

    if os.path.isdir(foldername):
        print ('foldername: [%s]' % foldername)
    else:
        print ('foldername: [' + foldername + '] is not a exist folder!')
        #return

    #keep show those image
    while(True):
        show(foldername)

if __name__ == "__main__":
    main()
    