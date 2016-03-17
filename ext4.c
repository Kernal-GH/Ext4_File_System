#include <iostream.h>
#include <io.h>
#include <fstream.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys\stat.h>
#include <time.h>
#include <math.h>
#include <iomanip.h>
#include <string.h>
#include <time.h>

#define DISKSIZE 1024*1024*10
#define BLOCKSIZE 1024
#define FCBSIZE 32
#define DATASIZE 10112
#define FAT1BEG 1
#define FAT2BEG 64
#define MAPBEG 126
#define DATABEG 128

struct diskinfo{//磁盘信息数据结构
	unsigned long size;//磁盘大小，单位字节
	unsigned long blocksize;//磁盘的盘块大小，单位字节
	unsigned long countofblock;//磁盘的盘块数
	unsigned long clustersize;//一簇的大小，单位盘块
	unsigned long usedspace;//已用空间，单位字节
	unsigned long remainspace;//剩余空间，单位字节
	unsigned long usedblock;//已用盘块
	unsigned long remainblock;//剩余盘块
};

struct directory{//目录项，即FCB
	char name[8];//文件前缀或目录名
	char type[3];//扩展名
	bool isdir;//目录标示符
	time_t buildtime;//创建时间
	time_t accesstime;//访问时间
	time_t modtime;//修改时间
	unsigned short int beginblock;//文件起始盘块或目录盘块
	long filesize;//文件大小或目录所含目录项数目
};

static char diskpath[]="SDisk.dat";//虚拟磁盘路径
static fstream disk;//虚拟磁盘
static diskinfo dinfo;//磁盘信息
static unsigned short int fat1[DATASIZE];//FAT表1
static unsigned short int fat2[DATASIZE];//FAT表2
static unsigned short int map[DATASIZE/16];//位示图
static int curblock;//当前目录盘块（相对位置）
static directory curdir[32],temp[32];//当前目录列表和临时目录列表
static char curpath[100];//当前路径
static char right[]=".",parent[]="..";
static char cmd[100];//指令
static char parameter[2][100];//参数列表
----------------------------------------------------<fs.cpp>-------------------------------------------------
#include "fs.h"

void init_diskinfo()
{//初始化磁盘信息
	dinfo.blocksize=BLOCKSIZE;//盘块大小，静态
	dinfo.clustersize=1;//毎簇大小，静态
	dinfo.countofblock=DATASIZE;//磁盘数据区盘块数，静态
	dinfo.size=DISKSIZE;//磁盘总大小，静态
	dinfo.usedblock=1;//已用盘块数，初始化时一盘块做根目录区，动态
	dinfo.remainblock=DATASIZE-1;//剩余盘块数
	dinfo.usedspace=BLOCKSIZE;//已用大小
	dinfo.remainspace=dinfo.size-dinfo.usedspace;//未用大小
}

void showdiskinfo()
{//输出磁盘信息
	struct _stat info;//文件信息
	_stat(diskpath, &info);//用于获取虚拟磁盘文件的时间信息
	cout<<"磁盘大小："<<dinfo.size<<"字节"<<endl;
	cout<<"盘块大小："<<dinfo.blocksize<<"字节"<<endl;
	cout<<"毎簇大小："<<dinfo.clustersize<<"盘块"<<endl;
	cout<<"实际空间："<<dinfo.countofblock*BLOCKSIZE<<"字节"<<endl;
	cout<<"已用空间："<<dinfo.usedspace<<"字节"<<endl;
	cout<<"剩余空间："<<dinfo.remainspace<<"字节"<<endl;
	cout<<"已用盘块："<<dinfo.usedblock<<endl;
	cout<<"剩余盘块："<<dinfo.remainblock<<endl;
	cout<<"文件管理系统：FAT16"<<endl;
	cout<<"磁盘创建时间："<<ctime(&info.st_ctime);//ctime将时间转换为可显示的格式输出
	cout<<"最后访问时间："<<ctime(&info.st_atime);
	cout<<"最后修改时间："<<ctime(&info.st_mtime);
}

void init_fat()
{//fat初始化，第一个盘块初始化为已用，置结束标识符ff0f，其余未用盘块置0
	fat1[0]=0xFF0F;
	fat2[0]=0xFF0F;
	for(int i=1;i<DATASIZE;i++){
		fat1[i]=0;
		fat2[i]=0;
	}
}
void showfat()
{//显示fat内容
	cout<<"Offset    0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F";
	cout.setf(ios::hex);//设置十六进制输出
	cout.setf(ios::fixed);//设置填充满域宽输出
	for(int i=0;i<DATASIZE;i++){
		if(!(i%16)){//一行输出16个盘块的fat
			cout<<endl;
			cout<<setfill('0')<<setw(3)<<setprecision(0)<<i/16<<"0"<<"   ";
		}
		cout<<setfill('0')<<setw(4)<<setprecision(0)<<fat1[i]<<" ";
	}
	cout<<endl;
	cout.unsetf(ios::hex);//解除十六进制输出标志位
	cout.unsetf(ios::fixed);//解除填充满域输出标志位
}

void init_map()
{//初始化位示图，第一个盘块置1，其余置0
	for(int i=0;i<DATASIZE/16-1;i++)
		map[i]=0;
	map[0] |= (1<<15);
}

void showmap()
{//显示位示图
	cout<<"Offset   00   10   20   30   40   50   60   70   80   90   A0   B0   C0   D0   E0   F0";
	cout.setf(ios::hex);//设置十六进制输出
	cout.setf(ios::fixed);//设置填充满域宽输出
	for(int i=0;i<DATASIZE/16;i++){
		if(!(i%16)){//一行输出16*16个盘块的位示图信息
			cout<<endl;
			cout<<setfill('0')<<setw(2)<<setprecision(0)<<i/16<<"00"<<"   ";
		}
		cout<<setfill('0')<<setw(4)<<setprecision(0)<<map[i]<<" ";
	}
	cout<<endl;
	cout.unsetf(ios::hex);//解除十六进制输出标志位
	cout.unsetf(ios::fixed);//解除填充满域输出标志位
}

void disktomem()
{//同步磁盘内容到内存中，包括磁盘信息，fat表，位示图和根目录
	disk.seekg(0,ios::beg);
	disk.read((unsigned char*)&dinfo,sizeof(diskinfo));
	disk.seekg(FAT1BEG*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)&fat1,sizeof(fat1));
	disk.seekg(FAT2BEG*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)&fat2,sizeof(fat2));
	disk.seekg(MAPBEG*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)&map,sizeof(map));
	disk.seekg(DATABEG*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)&curdir,sizeof(curdir));
}

void memtodisk()
{//同步内存中的磁盘信息、fat、map、根目录到磁盘上
	disk.seekp(0,ios::beg);
	disk.write((unsigned char*)&dinfo,sizeof(diskinfo));
	disk.seekp(FAT1BEG*BLOCKSIZE,ios::beg);
	disk.write((unsigned char*)&fat1,sizeof(fat1));
	disk.seekp(FAT2BEG*BLOCKSIZE,ios::beg);
	disk.write((unsigned char*)&fat2,sizeof(fat2));
	disk.seekp(MAPBEG*BLOCKSIZE,ios::beg);
	disk.write((unsigned char*)&map,sizeof(map));
}

bool init_disk()
{//建立虚拟磁盘的文件流，并返回是载人或是新建磁盘的bool型变量
	if(access(diskpath,0)!=-1){//如果磁盘已存在
		cout<<"Find existing disk:'SDisk.dat'"<<endl;
enter:	cout<<"Enter [Y] to Load it or [X] to empty it:";//提示用户是否载人已存在的磁盘，否则清空原有
		char ch;
		cin>>ch;
		cin.ignore(1000,'\n');//清空输入流
		if(ch=='y'||ch=='Y'){//用户选择载入磁盘
			cout<<"Load the existing disk..."<<endl;
			disk.open(diskpath,ios::in | ios::out | ios::binary);//、建立文件流
			return true;//返回
		}
		else if(ch=='x'||ch=='X')//用户选择新建磁盘
			;
		else{//字符不对应，提示重新输入
			cout<<"Wrong character:"<<ch<<endl;
			goto enter;
		}
	}
	char end=EOF;//文件结束标识符
	disk.open(diskpath,ios::trunc | ios::in | ios::out | ios::binary);//建立文件流
	disk.seekp(DISKSIZE,ios::beg);//写入文件结束标识符
	disk<<end;
	cout<<"Create new disk 'SDisk.dat'......"<<endl;
	return false;//返回
}

void init_dirtime(directory *dir)
{//初始化目录项的时间信息
	time(&(*dir).buildtime);
	time(&(*dir).accesstime);
	time(&(*dir).modtime);
}

void init_dir(directory *dir)
{//初始化新建目录的共同目录项字段
	(*dir).filesize=0;//目录在父目录下的目录项不显示本目录的目录项数目
	(*dir).isdir=true;
	(*dir).type[0]='\0';//目录项无扩展名
	init_dirtime(dir);//初始化目录项的时间信息
}

void init_root()
{//初始化根目录并同步到磁盘上
	curblock=0;//初始化当前目录在根目录
	strcpy(curdir[0].name,right);
	for(int i=2;i<32;i++)//初始化其他目录项为空
		curdir[i].name[0]=(char)0xE5;
	curdir[0].beginblock=0;//根目录
	init_dir(&curdir[0]);//初始化根目录的本目录项
	strcpy(curdir[0].name,right);
	memcpy(&curdir[1],&curdir[0],sizeof(directory));//初始化父目录的本目录项
	strcpy(curdir[1].name,parent);
	disk.seekp(DATABEG*BLOCKSIZE,ios::beg);
	disk.write((unsigned char*)&curdir,sizeof(curdir));//同步根目录到磁盘上
	curpath[0]='/';//初始化当前路径为根目录'/'
	curpath[1]='\0';
}

void showdir(int dirblock)
{//显示指定目录下内容
	disk.seekg((dirblock+DATABEG)*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)temp,sizeof(temp));//获取指定盘块下的目录
	cout<<"Name     Extension Position Size  CreateTime"<<endl;
	cout.setf(ios::left);
	for(int i=0;i<(temp[0].filesize+2);i++){
		if(strlen(temp[i].name)>8){//当文件名为8个字符时没有字符串结束符，须进行处理
			char name[10];
			strncpy(name,temp[i].name,8);
			name[8]='\0';
			cout<<setw(9)<<setfill(' ')<<name;
		}
		else
			cout<<setw(9)<<setfill(' ')<<temp[i].name;
		cout<<setw(10)<<setfill(' ')<<temp[i].type;
		cout<<setw(9)<<setfill(' ')<<temp[i].beginblock;
		cout<<setw(6)<<setfill(' ')<<temp[i].filesize;
		cout<<ctime(&temp[i].buildtime);
	}
	cout.unsetf(ios::left);
}

void init_fs()
{//初始化SDisk.dat及其文件系统
	if(init_disk()){//磁盘为载入
		disktomem();//从磁盘上同步信息到内存
		curblock=0;//初始化当前目录及路径
		curpath[0]='/';
		curpath[1]='\0';
	}
	else{//否则初始化磁盘信息、fat、map、根目录
		init_diskinfo();
		init_fat();
		init_map();
		init_root();
		memtodisk();//并同步到磁盘上
	}
}

unsigned short int allocate(unsigned int size)
{//分配size个盘块，修改fat、map、磁盘信息并返回size个盘块中的开始盘块号
	unsigned short int beginblock,lastblock;
	if(size<=0){//所申请要分配的空间必须大于0
		cout<<"The space apply for must be positive!"<<endl;
		return 0;
	}
	if(dinfo.remainblock < size){//没有足够的空间
		cout<<"No enough space!"<<endl;
		return 0;
	}
	for(unsigned int i=0,j=0;i<DATASIZE&&j<size;i++){//遍历位示图
		if(map[i/16]&(1<<(15-i%16)))//盘块已用
			continue;
		else{//找到空闲盘块
			map[i/16]|=(1<<(15-i%16));//map相应位置1
			if((j++)==0)//当前分配的是第一个盘块
				beginblock=i;//置开始盘块为i
			else
				fat1[lastblock]=fat2[lastblock]=i;//否则置上次分配的盘块对应的fat项为i
			lastblock=i;//记录当前盘块以备分配下个盘块用
		}
	}
	fat1[lastblock]=fat2[lastblock]=0xFF0F;//最后一个盘块置文件结束符
	dinfo.usedblock+=size;//更新磁盘信息
	dinfo.remainblock-=size;
	dinfo.usedspace+=size*BLOCKSIZE;
	dinfo.remainspace-=size*BLOCKSIZE;
	memtodisk();//同步到磁盘
	return beginblock;//返回分配盘块的开始盘块
}

bool analysisname(char *filename,char *name,char *type)
{//将字符串文件名分解成文件名和扩展名
	unsigned int i;
	for(i=0;(filename[i]!='.')&&(i<strlen(filename));i++)
		;//定位到文件名和扩展名分隔符
	if(i>8){//文件名不能多于8位
		cout<<"ERROR!Length of file name must less than 8!"<<endl;
		return false;
	}
	if(filename[i]=='.'&&(strlen(&filename[i+1])>3)){//扩展名不能多于3位
		cout<<"ERROR!Length of file extension must less than 3!"<<endl;
		return false;
	}
	if(i==strlen(filename)){//无扩展名
		strcpy(name,filename);
		type[0]='\0';
	}
	else{//有扩展名
		strncpy(name,filename,i);
		name[i]='\0';
		strcpy(type,&filename[i+1]);
		type[strlen(&filename[i+1])]='\0';
	}
	return true;
}

bool isName(bool isdir,char *filename)
{//文件名和目录名中是否有非法字符
	unsigned i,j;
	for(i=0;(filename[i]!='.')&&(i<strlen(filename));i++){//找到文件名和扩展名分隔符'.'
		if(filename[i]=='/')//文件名或者目录名中有目录项分隔符/
			return false;
	}
	if(isdir&&i!=strlen(filename))//目录名中有'.'
		return false;
	for(j=i+1;j<strlen(filename);j++){
		if(filename[j]=='/'||filename[j]=='.')//文件扩展名中有非法字符
			return false;
	}
	return true;
}

void recycle(int dirblock,int pos)
{//回收指定的空间，递归
	int size=0;//记录回收的盘块数
	disk.seekg((dirblock+DATABEG)*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)temp,sizeof(temp));
	directory tempdir;
	memcpy(&tempdir,&temp[pos],sizeof(directory));//暂存所要删除的目录项
	temp[pos].name[0]=(char)0xE5;//清除目录项
	temp[0].filesize--;//本目录更新数据
	if(pos!=temp[0].filesize+2)//保持目录项的连贯
		memcpy(&temp[pos],&temp[pos+1],(temp[0].filesize+2-pos)*sizeof(directory));
	disk.seekp((dirblock+DATABEG)*BLOCKSIZE,ios::beg);//更新到磁盘中
	disk.write((unsigned char*)temp,sizeof(temp));
	if(tempdir.isdir){//删除子目录下的内容
		size+=1;//回收目录盘块+1
		map[tempdir.beginblock/16]&=(~(1<<(15-tempdir.beginblock%16)));//置位示图相应位置
		fat1[tempdir.beginblock]=fat2[tempdir.beginblock]=0;//置fat相应位置
		disk.seekg((tempdir.beginblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)temp,sizeof(temp));//获取子目录的目录项		
		for(int i=temp[0].filesize+1;i>1;i--)
			recycle(temp[0].beginblock,i);//递归回收目录下的内容
	}
	else{//回收文件
		size+=temp[pos].filesize;//回收文件，回收大小加文件大小
		int thisblock=tempdir.beginblock,nextblock;
		for(int i=0;i<tempdir.filesize;i++){//沿fat指示回收盘块
			nextblock=fat1[thisblock];//记录下一个要回收的盘块
			map[thisblock/16]&=(~(1<<(15-thisblock%16)));//置map位
			fat1[thisblock]=fat2[thisblock]=0;//置fat位
			thisblock=nextblock;
		}
	}
	dinfo.usedblock-=size;//更新磁盘信息
	dinfo.remainblock+=size;
	dinfo.usedspace-=size*BLOCKSIZE;
	dinfo.remainspace+=size*BLOCKSIZE;
	memtodisk();//同步到磁盘上
}

int find(char *filename,int block)
{//根据文件名在block所指目录盘块下找到文件目录项
	disk.seekg((block+DATABEG)*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)temp,sizeof(temp));
	if(strcmp(filename,"..")==0)//为父目录
		return 1;
	if(strcmp(filename,".")==0)//为本目录
		return 0;
	for(int i=0;i<temp[0].filesize+2;i++){
		if(temp[i].isdir&&(strcmp(filename,temp[i].name)==0))//目录直接比较目录名
			return i;
		else{//若为文件则将文件名扩展名合并比较
			char tempname[15];
			if(strlen(temp[i].name)>8){//文件名为8位时做特殊处理
				strncpy(tempname,temp[i].name,8);
				tempname[8]='\0';
			}
			else
				strcpy(tempname,temp[i].name);
			if(strlen(temp[i].type)!=0){//文件有扩展名
				strcpy(&tempname[strlen(tempname)+1],temp[i].type);
				tempname[strlen(tempname)]='.';
			}
			if(strcmp(tempname,filename)==0)//比较文件名
				return i;
		}			
	}
	return -1;
}

void createdir(char* name)
{//创建目录
	if(curdir[0].filesize==30){//目录中目录项已满
		cout<<"The current directory already has 30 directories!Cannot create directory or file anymore!!"<<endl;
		return;
	}
	if(strlen(name)>8){//名字大于8位
		cout<<"The directory name must not bigger than 8 letters!"<<endl;
		return;
	}
	if(!isName(true,name)){//目录名中是否含有非法字符
		cout<<"The directory name has illagel character."<<endl;
		return;
	}
	if(find(name,curblock)!=-1){//在当前目录下有重名的文件或目录
		cout<<"The directory:"<<name<<" is existing."<<endl;
		return;
	}
	int beginblock=allocate(1);//分配文件空间
	if(!beginblock){//分配失败，退出
		cout<<"Failed to create new directory!"<<endl;
		return;
	}
	int pos=(curdir[0].filesize++)+2;
	curdir[pos].beginblock=beginblock;
	strcpy(curdir[pos].name,name);
	init_dir(&curdir[pos]);//初始化目录项
	disk.seekp((curblock+DATABEG)*BLOCKSIZE);
	disk.write((unsigned char*)curdir,sizeof(curdir));//同步目录到磁盘
	memcpy(&temp[0],&curdir[curdir[0].filesize+1],sizeof(directory));//新建目录第一项初始化
	memcpy(&temp[1],&curdir[0],sizeof(directory));//将新建目录第二项初始化
	temp[1].filesize=0;//子目录无法得知父目录下的目录项个数
	strcpy(temp[0].name,right);
	strcpy(temp[1].name,parent);
	for(int i=2;i<32;i++)//初始化新建目录
		temp[i].name[0]=(char)0xE5;
	disk.seekp((curdir[pos].beginblock+DATABEG)*BLOCKSIZE,ios::beg);
	disk.write((unsigned char*)temp,sizeof(temp));//同步新建目录到磁盘
	disk.seekg((curdir[pos].beginblock+DATABEG)*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)temp,sizeof(temp));//同步新建目录到磁盘
}

void rmdir(int pos)
{//删除目录
	if(curdir[pos].isdir){//删除目录项为目录
		recycle(curblock,pos);//回收目录
		disk.seekg((curblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)curdir,sizeof(curdir));//磁盘内存同步
	}
	else//目录项不为目录，报错
		cout<<curdir[pos].name<<"is not a directory."<<endl;
}



void createfile(char* filename,char *type,int size)
{//创建文件
	if(curdir[0].filesize==30){//目录中目录项已满
		cout<<"The current directory already has 30 directories!Cannot create directory or file anymore!!"<<endl;
		return;
	}
	int beginblock=allocate(size);//分配文件空间
	if(!beginblock){//分配失败，退出
		cout<<"Failed to create new file!"<<endl;
		return;
	}
	int pos=(curdir[0].filesize++)+2;
	curdir[pos].beginblock=beginblock;//初始化文件的目录项
	strcpy(curdir[pos].name,filename);
	strcpy(curdir[pos].type,type);
	curdir[pos].isdir=false;
	curdir[pos].filesize=size;
	init_dirtime(&curdir[pos]);
	disk.seekp((curblock+DATABEG)*BLOCKSIZE,ios::beg);
	disk.write((unsigned char*)curdir,sizeof(curdir));//同步目录到磁盘
}

void delfile(int pos)
{//删除文件
	if(curdir[pos].isdir)//删除的目录项不能为目录
		cout<<curdir[pos].name<<" is not a file."<<endl;
	else{//删除的目录项是文件
		recycle(curblock,pos);//回收文件所占空间
		disk.seekg((curblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)curdir,sizeof(curdir));//磁盘与内存同步
	}
}

bool analysispath(char *path,int *block,int *pos)
{//分析字符串形式路径，返回路径指向的目录项的盘块和位置
	char filename[15];
	int i;
	int len=strlen(path);
	*pos=0;//所指路径指向文件时才置pos为非0值
	if(path[0]=='/'){//绝对路径
		*block=0;
		if(strlen(path)==1)//是根目录
			return true;
		else{//绝对路径
			if(path[1]!='/'&&analysispath(&path[1],block,pos))
				return true;
		}
		return false;
	}
	else{//相对路径
		for(i=0;i<len&&path[i]!='/';i++)
			;//获取所要寻找的本级目录的目录项名及其长度
		strncpy(filename,path,i);
		filename[i]='\0';	
		*pos=find(filename,*block);//在目录盘块中找到目录项
		if(*pos==-1)//找不到文件
			return false;
	}
	if(i==len||i==len-1)//路径分析完毕
		return true;
	else{//路径未分析完
		if(!temp[*pos].isdir){//此时找到的目录项是文件，报错
			cout<<"File:"<<filename<<" is not a directory!"<<endl;
			return false;
		}
		if(path[i+1]=='/'){//路径中重复了目录项分隔符'/'
			cout<<"The path is illegal."<<endl;
			return false;
		}
		*block=temp[*pos].beginblock;//从下级目录开始分析路径
		if(analysispath(&path[i+1],block,pos))//递归分析后续路径
			return true;
		else
			return false;
	}
}

bool isdir(int block,int pos)
{//判断目录项是否为目录
	disk.seekg((block+DATABEG)*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)temp,sizeof(temp));
	if(temp[pos].isdir)//路径所指为目录,不能为文件
		return true;
	else
		return false;
}

void getcurpath()
{//获取当前路径
	strcpy(curpath,"/");//根目录
	curpath[1]='\0';
	if(curblock==0)//当前为根目录
		return;
	char path[30][10];
	int i,j,block;
	memcpy(temp,curdir,sizeof(temp));
	for(i=0;temp[0].beginblock!=0;i++){//获取当前路径中各目录项的目录名
		block=temp[0].beginblock;
		disk.seekg((temp[1].beginblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)temp,sizeof(temp));
		for(j=2;j<temp[0].filesize+2;j++){
			if(block==temp[j].beginblock){//找到目录名复制到暂存的路径数组中
				strcpy(path[i],temp[j].name);
			}
		}
	}
	for(j=i;j>0;j--){//将路径数组中的目录名组织成当前路径
		strcat(curpath,path[j-1]);
		strcat(curpath,"/");
	}
}

void changedir(char* path)
{//改变当前目录，更新当前路径
	int block,pos=0;
	block=curblock;
	if(analysispath(path,&block,&pos)){//路径存在
		disk.seekg((block+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)temp,sizeof(temp));
		if(temp[pos].isdir){//路径所指为目录,不能为文件
			curblock=temp[pos].beginblock;//重置当前目录相关的全局变量
			disk.seekg((curblock+DATABEG)*BLOCKSIZE,ios::beg);
			disk.read((unsigned char*)curdir,sizeof(curdir));
			getcurpath();//获取当前路径
			return;
		}
	}
	cout<<"Path:"<<path<<" is not exist or not a directory!"<<endl;
}


void copyfile(char* src,char *den)
{//复制文件
	int srcblock,srcpos=0;
	int denblock,denpos=0;
	srcblock=curblock;
	denblock=curblock;
	directory srcdir;
	if(analysispath(src,&srcblock,&srcpos)){//源文件存在
		disk.seekg((srcblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)temp,sizeof(temp));
		if(temp[srcpos].isdir){//只能复制文件
			cout<<"Cannot copy directory."<<endl;
			return;
		}
		disk.seekg((srcblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)temp,sizeof(temp));
		memcpy(&srcdir,&temp[srcpos],sizeof(directory));//暂存找到的文件目录项
		if(analysispath(den,&denblock,&denpos)){//目的路径存在
			disk.seekg((denblock+DATABEG)*BLOCKSIZE,ios::beg);
			disk.read((unsigned char*)temp,sizeof(temp));
			if(!temp[denpos].isdir){//目的路径必须指向目录
				cout<<"The destinate path shouldnot be a file."<<endl;
				return;
			}
			srcblock=curblock;
			disk.seekg((temp[denpos].beginblock+DATABEG)*BLOCKSIZE,ios::beg);
			curblock=temp[denpos].beginblock;
			memcpy(temp,curdir,sizeof(curdir));//保存当前目录以备还原
			disk.read((unsigned char*)curdir,sizeof(curdir));
			for(int i=2;i<curdir[0].filesize+2;i++){//目的路径下有重名文件
				if((strcmp(srcdir.name,temp[i].name)==0)&&(strcmp(srcdir.type,temp[i].type)==0)){
					cout<<"The file is existing in the destinate directory."<<endl;
					return;
				}
			}
			createfile(srcdir.name,srcdir.type,srcdir.filesize);//创建文件
			curblock=srcblock;//还原当前目录
			memcpy(curdir,temp,sizeof(curdir));
			return;
		}
		else//目的路径不存在
			cout<<"Destination path:"<<den<<" is not exist!"<<endl;
	}
	else//源文件不存在
		cout<<"Source path:"<<src<<" is not exist!"<<endl;
}

void rename(char *oldname,char *newname)
{//重命名文件
	int pos=find(oldname,curblock);//找到文件
	if(pos==-1)
		return;
	if(analysisname(newname,curdir[pos].name,curdir[pos].type)){
		//同步到磁盘并重新读取
		disk.seekp((curblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.write((unsigned char*)curdir,sizeof(curdir));
		disk.seekg((curblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)curdir,sizeof(curdir));
	}
	else
		cout<<"Failed to rename the file:"<<oldname<<endl;
}

void showhelpinfo()
{//显示文件系统的帮助信息
	cout<< "==================================================\n";
	cout<< "              FAT文件系统命令菜单             \n";		
	cout<< "   dir                         显示目录内容\n";
	cout<< "   cd [Path]                   目录切换\n";
	cout<< "   mkdir [Dirname]             创建目录\n";
	cout<< "   rmdir [Dirname]             删除目录\n";
	cout<< "   mkfile [Filename][Size]     创建文件\n";
	cout<< "   rename [oldname] [newname]  文件或目录重命名\n";
	cout<< "   cpfile [FilePath] [DirPath] 复制文件\n";
	cout<< "   del [Filename]|[Path]       删除文件\n";
	cout<< "   showdisk                    显示磁盘信息\n";
	cout<< "   showfat                     显示FAT\n";
	cout<< "   showmap                     显示位示图map\n";
	cout<< "   format                      格式化磁盘\n";
	cout<< "   clear                       清屏\n";
	cout<< "   help                        显示帮助\n";
	cout<< "   exit                        退出\n";
	cout<< "=================================================\n";
}

bool getpara(int num)
{//获取参数
	int i,j,k;
	char temp[100];
	cin.getline(temp,99);
	int length=strlen(temp);
	bool pa=false;
	for(i=0,j=0;i<length;i++){//计算获取的参数个数
		if(temp[i]==' ')
			pa=true;
		if(temp[i]!=' '&&pa){
			j++;
			pa=false;
		}
	}
	if(j!=num){//参数个数错误
		cout<<"Command '"<<cmd<<"' doesn't take "<<j<<" parameters!"<<endl;
		cout<<"Please enter'help' to get help information."<<endl;
		return false;
	}
	for(i=0,j=0,k=0;i<length;i++){//复制参数到参数列表
		if(temp[i]==' '){
			if(j>0){
				parameter[k++][j]='\0';
				j=0;
			}
			continue;
		}
		else
			parameter[k][j++]=temp[i];
	}
	if(j>0)
		parameter[k++][j]='\0';
	return true;
}

void get_command()
{//获取用户输入的命令并执行相应动作
	bool exit=false;
	while(!exit){
		cout<<"[Path: "<<curpath<<" ]# ";//输出当前路径和等待命令输入符号#
		cin>>cmd;//获取命令
		if(strlen(cmd)==0)//命令不能为空
			continue;
		if(strcmp(cmd,"dir")==0){//命令为dir
			if(getpara(0))
				showdir(curblock);//显示当前目录信息
		}
		else if(strcmp(cmd,"cd")==0){//命令为cd
			if(getpara(1))
				changedir(parameter[0]);//改变当前目录
		}
		else if(strcmp(cmd,"mkdir")==0){//命令为mkdir
			if(getpara(1))
				createdir(parameter[0]);//在当前目录下创建目录
		}
		else if(strcmp(cmd,"rmdir")==0){//命令为rmdir
			if(getpara(1)){
				int pos;
				if(strcmp(parameter[0],"..")==0)//不能删除父目录
					cout<<"Cannot remove the parent directory."<<endl;
				else if(strcmp(parameter[0],".")==0)//不能删除本目录
					cout<<"Cannot remove the current directory."<<endl;
				else{
					pos=find(parameter[0],curblock);
					if(pos==-1)//子目录不存在
						cout<<"The directory:'"<<parameter[0]<<"' is not exist in the current directory!"<<endl;
					else
						rmdir(pos);//子目录存在，删除子目录
				}
			}
		}
		else if(strcmp(cmd,"mkfile")==0){//命令为mkfile
			if(getpara(2)){
				char name[10],type[5];
				if(isName(false,parameter[0])){//文件名不含非法字符
					if(find(parameter[0],curblock)==-1){//当前目录没有重名文件
						if(analysisname(parameter[0],name,type))
							createfile(name,type,atoi(parameter[1]));//在当前目录下创建新文件
					}
					else
						cout<<"The file:"<<parameter[0]<<" is existing."<<endl;
				}
				else
					cout<<"The path:"<<parameter[0]<<" has illagel character."<<endl;
			}
		}
		else if(strcmp(cmd,"rename")==0){//命令为rename
			if(getpara(2))
				rename(parameter[0],parameter[1]);//重命名文件
		}
		else if(strcmp(cmd,"cpfile")==0){//命令为cpfile
			if(getpara(2))
				copyfile(parameter[0],parameter[1]);//复制文件
		}
		else if(strcmp(cmd,"del")==0){//命令为del
			if(getpara(1)){
				int pos;
				pos=find(parameter[0],curblock);
				if(pos==-1)//当前目录下没有该文件
					cout<<"File:"<<parameter[0]<<" is not exist in the current directory."<<endl;
				else
					delfile(pos);//文件存在，删除文件
			}
		}
		else if(strcmp(cmd,"showdisk")==0){//命令为showdisk
			if(getpara(0))
				showdiskinfo();//显示磁盘信息
		}
		else if(strcmp(cmd,"showfat")==0){//命令为showfat
			if(getpara(0))
				showfat();//显示FAT表
		}
		else if(strcmp(cmd,"showmap")==0){//命令为showmap
			if(getpara(0))
				showmap();//显示位示图
		}
		else if(strcmp(cmd,"format")==0){//命令为format
			if(getpara(0)){
format:			cout<<"Format disk will lost all file! Please make sure.[Y/N]:";
				char ch;
				cin>>ch;
				cin.ignore(1000,'\n');
				if(ch=='Y'||ch=='y'){//格式化磁盘
					disk.close();//关闭文件流
					remove(diskpath);//删除文件
					init_fs();//初始化新磁盘
					cout<<"Disk has been formated."<<endl;
				}
				else if(ch=='N'||ch=='n')
					cout<<"Cancel format disk."<<endl;
				else{
					cout<<"Wrong character.Please enter[Y/N] again."<<endl;	
					goto format;
				}
			}
		}
		else if(strcmp(cmd,"clear")==0){//命令为clear
			if(getpara(0))
				system("cls");//清屏
		}
		else if(strcmp(cmd,"help")==0){//命令为help
			if(getpara(0))
				showhelpinfo();//显示帮助信息
		}
		else if(strcmp(cmd,"exit")==0){//命令为exit
			if(getpara(0)){
				disk.close();//关闭磁盘
				exit=true;
			}
		}
		else{//命令不存在
			cout<<"'"<<cmd<<"' is not a valid commmand."<<endl;
			cin.ignore(1000,'\n');
		}
		if(exit==true)//判断是否退出程序
			cout<<"Exit the filesystem......"<<endl;
		cmd[0]=parameter[0][0]=parameter[1][0]='\0';//清空命令字符串和参数列表
	}
}

void main()
{//主函数
	init_fs();//初始化虚拟磁盘和文件系统
	get_command();//等待用户操作命令
}
