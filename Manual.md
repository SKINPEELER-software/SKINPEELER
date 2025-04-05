# SKINPEELER v1.0 Manual

## 1. Open File

1) Click the [Open DICOM] button to select the folder containing the Dicom File(.dcm) you want to peel.
2) Only Transverse CT Files should exist in the selected folder.
3) Subfolders may exist in the selected folder, but files within subfolders will be excluded.
4) When file opening is complete, the image of the first file will be displayed along with the Current Directory/Current File information.
5) You can navigate between files using the Prev/Next buttons or the left/right keyboard arrow keys.
6) CT Image can be zoomed in/out using the mouse wheel, and you can move the image by clicking and dragging.

## 2. Skin Peeling

1) Click the [Peel Skin] checkbox.
2) Select your desired Peeling Mode:
   
   ### 2-1) Auto
   Auto Mode peels the remaining CT slices based on the CT you're currently viewing.

   1) When you click [Auto], the Current Skin Thickness estimated from the current CT will be displayed.
   2) You can adjust the Skin Thickness using the [Decrease by 1]/[Increase by 1] buttons.
   3) Press the [Peel Current Slice] button to peel the current slice by the current thickness.
   4) Press the [Peel All] button to peel the remaining CT slices.
      - This will apply the estimated Skin Thickness plus any additional thickness you increased/decreased in step 2).
      - For example, if the estimated Skin Thickness of the current CT is 3, and you increased it by 2, the total Peeling Thickness will be 5.
      - When you press [Peel All], if another CT's estimated Skin Thickness is 2, the additional thickness you increased (= 2) will be added.
      - Thus, it will peel with a thickness of 4.

   ### 2-2) Uniform
   Uniform Mode allows you to define the thickness on an arbitrary number of slices and peel the rest based on these definitions.

   1) When you click [Uniform], the #Slice field and two blank fields will be activated.
   2) Enter the number N of slices where you want to define thickness in the right blank field and press Enter.
   3) N slices will be designated as Sample Slices at equal intervals throughout the entire CT set.
   4) You can adjust the Skin Thickness using the [Decrease by 1]/[Increase by 1] buttons.
   5) Press the [Peel Current Slice] button to peel by the current thickness and move to the next Sample Slice.
   6) After completing steps 4) - 5) for all N Sample Slices, the [Prev Slice]/[Next Slice] and [Peel All] buttons will be activated.
   7) You can navigate between Sample Slices using the [Prev Slice]/[Next Slice] buttons.
   8) Press the [Peel All] button to peel the remaining CT slices.

3) When peeling is complete, information about the peeling will be displayed in the File Statistics section.
4) After peeling is complete, you can check the results of other CT files using the [Prev]/[Next] or [Prev Slice]/[Next Slice] buttons.

## 3. File Statistics
When peeling is complete, information will be displayed in the File Statistics section.

1) You can save the File Statistics using the [Save Statistics] button.

## 4. Save as DICOM
Save the peeled CT.

1) Click the [Save as DICOM] button to select the folder where you want to save the peeled CT.
2) The saved CT file will be named "peeled-(original file name).dcm".
