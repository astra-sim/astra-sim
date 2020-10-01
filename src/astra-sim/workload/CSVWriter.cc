/******************************************************************************
Copyright (c) 2020 Georgia Institute of Technology
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Author : Saeed Rashidi (saeed.rashidi@gatech.edu)
*******************************************************************************/
#include "CSVWriter.hh"
namespace AstraSim{
CSVWriter::CSVWriter(std::string path,std::string name) {
    this->path=path;
    this->name=name;
}
void CSVWriter::initialize_csv(int rows, int cols) {
    std::cout<<"path to create csvs is: "<<path<<std::endl;
    do{
       std::cout << "trying to open: " << path << std::endl;
       myFile.open(path+name, std::fstream::out);
    }while (!myFile.is_open());
    do{
        myFile.close();
    }while (myFile.is_open());

    do{
        std::cout << "trying to open: " << path << std::endl;
        myFile.open(path+name, std::fstream::out | std::fstream::in);
    }while (!myFile.is_open());

    if (!myFile) {
        std::cout << "Unable to open file: " << path << std::endl;
    } else {
        std::cout << "success in openning file" << std::endl;
    }

    myFile.seekp(0,std::ios_base::beg);
    myFile.seekg(0,std::ios_base::beg);
    /*if(!File.eof()){
        std::cout<<"eof reached"<<std::endl;
        return;
    }*/
    for(int i=0;i<rows;i++){
        for(int j=0;j<cols-1;j++){
            myFile<<',';
        }
        myFile<<'\n';
    }
    do{
        myFile.close();
    }while (myFile.is_open());

}
void CSVWriter::write_cell(int row, int column,std::string data) {
    std::string str="";
    std::string tmp;

    int status=1;
    int fildes=-1;
    do {
        fildes = open((path+name).c_str(), O_RDWR);
    }while(fildes==-1);
    
    do{
        status = lockf(fildes, F_TLOCK, (off_t)1000000);
    }while(status!=0);
    char buf[1];
    while(row>0){
        status=read(fildes,buf,1);
        str=str+(*buf);
        if(*buf=='\n'){
            row--;
        }
    }
    while(column>0){
        status=read(fildes,buf,1);
        str=str+(*buf);
        if(*buf==','){
            column--;
        }
        if(*buf=='\n'){
            std::cout<<"fatal error in inserting cewll!"<<std::endl;
        }
    }
    str=str+data;
    while(read(fildes,buf,1)){
        str=str+(*buf);
    }

    do{
        lseek(fildes,0,SEEK_SET);
       status=write(fildes,str.c_str(),str.length());
    }while (status!=str.length());

    do {
        status=lseek(fildes, 0, SEEK_SET);
    }while(status==-1);

    do{
        status = lockf(fildes, F_ULOCK, (off_t)1000000);
    }while(status!=0);

    do {
        status = close(fildes);
    }while(status==-1);
    return;
}

}
