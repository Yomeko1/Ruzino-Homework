# Building

1. Download SDK
![alt text](images/image-1.png)
```
python .\configure.py --all --build_variant Debug --extract-sdk .\SDK.zip
``` 

```
mkdir build
cd build
cmake .. -G Ninja
ninja
```
```
cd Binaries/Debug
.\Ruzino.exe
```
![alt text](images/image-2.png)
![alt text](images/image-3.png)
![alt text](images/image-4.png)
![alt text](images/image-5.png)

