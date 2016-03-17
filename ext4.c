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

struct diskinfo{//������Ϣ���ݽṹ
	unsigned long size;//���̴�С����λ�ֽ�
	unsigned long blocksize;//���̵��̿��С����λ�ֽ�
	unsigned long countofblock;//���̵��̿���
	unsigned long clustersize;//һ�صĴ�С����λ�̿�
	unsigned long usedspace;//���ÿռ䣬��λ�ֽ�
	unsigned long remainspace;//ʣ��ռ䣬��λ�ֽ�
	unsigned long usedblock;//�����̿�
	unsigned long remainblock;//ʣ���̿�
};

struct directory{//Ŀ¼���FCB
	char name[8];//�ļ�ǰ׺��Ŀ¼��
	char type[3];//��չ��
	bool isdir;//Ŀ¼��ʾ��
	time_t buildtime;//����ʱ��
	time_t accesstime;//����ʱ��
	time_t modtime;//�޸�ʱ��
	unsigned short int beginblock;//�ļ���ʼ�̿��Ŀ¼�̿�
	long filesize;//�ļ���С��Ŀ¼����Ŀ¼����Ŀ
};

static char diskpath[]="SDisk.dat";//�������·��
static fstream disk;//�������
static diskinfo dinfo;//������Ϣ
static unsigned short int fat1[DATASIZE];//FAT��1
static unsigned short int fat2[DATASIZE];//FAT��2
static unsigned short int map[DATASIZE/16];//λʾͼ
static int curblock;//��ǰĿ¼�̿飨���λ�ã�
static directory curdir[32],temp[32];//��ǰĿ¼�б����ʱĿ¼�б�
static char curpath[100];//��ǰ·��
static char right[]=".",parent[]="..";
static char cmd[100];//ָ��
static char parameter[2][100];//�����б�
----------------------------------------------------<fs.cpp>-------------------------------------------------
#include "fs.h"

void init_diskinfo()
{//��ʼ��������Ϣ
	dinfo.blocksize=BLOCKSIZE;//�̿��С����̬
	dinfo.clustersize=1;//���ش�С����̬
	dinfo.countofblock=DATASIZE;//�����������̿�������̬
	dinfo.size=DISKSIZE;//�����ܴ�С����̬
	dinfo.usedblock=1;//�����̿�������ʼ��ʱһ�̿�����Ŀ¼������̬
	dinfo.remainblock=DATASIZE-1;//ʣ���̿���
	dinfo.usedspace=BLOCKSIZE;//���ô�С
	dinfo.remainspace=dinfo.size-dinfo.usedspace;//δ�ô�С
}

void showdiskinfo()
{//���������Ϣ
	struct _stat info;//�ļ���Ϣ
	_stat(diskpath, &info);//���ڻ�ȡ��������ļ���ʱ����Ϣ
	cout<<"���̴�С��"<<dinfo.size<<"�ֽ�"<<endl;
	cout<<"�̿��С��"<<dinfo.blocksize<<"�ֽ�"<<endl;
	cout<<"���ش�С��"<<dinfo.clustersize<<"�̿�"<<endl;
	cout<<"ʵ�ʿռ䣺"<<dinfo.countofblock*BLOCKSIZE<<"�ֽ�"<<endl;
	cout<<"���ÿռ䣺"<<dinfo.usedspace<<"�ֽ�"<<endl;
	cout<<"ʣ��ռ䣺"<<dinfo.remainspace<<"�ֽ�"<<endl;
	cout<<"�����̿飺"<<dinfo.usedblock<<endl;
	cout<<"ʣ���̿飺"<<dinfo.remainblock<<endl;
	cout<<"�ļ�����ϵͳ��FAT16"<<endl;
	cout<<"���̴���ʱ�䣺"<<ctime(&info.st_ctime);//ctime��ʱ��ת��Ϊ����ʾ�ĸ�ʽ���
	cout<<"������ʱ�䣺"<<ctime(&info.st_atime);
	cout<<"����޸�ʱ�䣺"<<ctime(&info.st_mtime);
}

void init_fat()
{//fat��ʼ������һ���̿��ʼ��Ϊ���ã��ý�����ʶ��ff0f������δ���̿���0
	fat1[0]=0xFF0F;
	fat2[0]=0xFF0F;
	for(int i=1;i<DATASIZE;i++){
		fat1[i]=0;
		fat2[i]=0;
	}
}
void showfat()
{//��ʾfat����
	cout<<"Offset    0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F";
	cout.setf(ios::hex);//����ʮ���������
	cout.setf(ios::fixed);//���������������
	for(int i=0;i<DATASIZE;i++){
		if(!(i%16)){//һ�����16���̿��fat
			cout<<endl;
			cout<<setfill('0')<<setw(3)<<setprecision(0)<<i/16<<"0"<<"   ";
		}
		cout<<setfill('0')<<setw(4)<<setprecision(0)<<fat1[i]<<" ";
	}
	cout<<endl;
	cout.unsetf(ios::hex);//���ʮ�����������־λ
	cout.unsetf(ios::fixed);//���������������־λ
}

void init_map()
{//��ʼ��λʾͼ����һ���̿���1��������0
	for(int i=0;i<DATASIZE/16-1;i++)
		map[i]=0;
	map[0] |= (1<<15);
}

void showmap()
{//��ʾλʾͼ
	cout<<"Offset   00   10   20   30   40   50   60   70   80   90   A0   B0   C0   D0   E0   F0";
	cout.setf(ios::hex);//����ʮ���������
	cout.setf(ios::fixed);//���������������
	for(int i=0;i<DATASIZE/16;i++){
		if(!(i%16)){//һ�����16*16���̿��λʾͼ��Ϣ
			cout<<endl;
			cout<<setfill('0')<<setw(2)<<setprecision(0)<<i/16<<"00"<<"   ";
		}
		cout<<setfill('0')<<setw(4)<<setprecision(0)<<map[i]<<" ";
	}
	cout<<endl;
	cout.unsetf(ios::hex);//���ʮ�����������־λ
	cout.unsetf(ios::fixed);//���������������־λ
}

void disktomem()
{//ͬ���������ݵ��ڴ��У�����������Ϣ��fat��λʾͼ�͸�Ŀ¼
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
{//ͬ���ڴ��еĴ�����Ϣ��fat��map����Ŀ¼��������
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
{//����������̵��ļ����������������˻����½����̵�bool�ͱ���
	if(access(diskpath,0)!=-1){//��������Ѵ���
		cout<<"Find existing disk:'SDisk.dat'"<<endl;
enter:	cout<<"Enter [Y] to Load it or [X] to empty it:";//��ʾ�û��Ƿ������Ѵ��ڵĴ��̣��������ԭ��
		char ch;
		cin>>ch;
		cin.ignore(1000,'\n');//���������
		if(ch=='y'||ch=='Y'){//�û�ѡ���������
			cout<<"Load the existing disk..."<<endl;
			disk.open(diskpath,ios::in | ios::out | ios::binary);//�������ļ���
			return true;//����
		}
		else if(ch=='x'||ch=='X')//�û�ѡ���½�����
			;
		else{//�ַ�����Ӧ����ʾ��������
			cout<<"Wrong character:"<<ch<<endl;
			goto enter;
		}
	}
	char end=EOF;//�ļ�������ʶ��
	disk.open(diskpath,ios::trunc | ios::in | ios::out | ios::binary);//�����ļ���
	disk.seekp(DISKSIZE,ios::beg);//д���ļ�������ʶ��
	disk<<end;
	cout<<"Create new disk 'SDisk.dat'......"<<endl;
	return false;//����
}

void init_dirtime(directory *dir)
{//��ʼ��Ŀ¼���ʱ����Ϣ
	time(&(*dir).buildtime);
	time(&(*dir).accesstime);
	time(&(*dir).modtime);
}

void init_dir(directory *dir)
{//��ʼ���½�Ŀ¼�Ĺ�ͬĿ¼���ֶ�
	(*dir).filesize=0;//Ŀ¼�ڸ�Ŀ¼�µ�Ŀ¼���ʾ��Ŀ¼��Ŀ¼����Ŀ
	(*dir).isdir=true;
	(*dir).type[0]='\0';//Ŀ¼������չ��
	init_dirtime(dir);//��ʼ��Ŀ¼���ʱ����Ϣ
}

void init_root()
{//��ʼ����Ŀ¼��ͬ����������
	curblock=0;//��ʼ����ǰĿ¼�ڸ�Ŀ¼
	strcpy(curdir[0].name,right);
	for(int i=2;i<32;i++)//��ʼ������Ŀ¼��Ϊ��
		curdir[i].name[0]=(char)0xE5;
	curdir[0].beginblock=0;//��Ŀ¼
	init_dir(&curdir[0]);//��ʼ����Ŀ¼�ı�Ŀ¼��
	strcpy(curdir[0].name,right);
	memcpy(&curdir[1],&curdir[0],sizeof(directory));//��ʼ����Ŀ¼�ı�Ŀ¼��
	strcpy(curdir[1].name,parent);
	disk.seekp(DATABEG*BLOCKSIZE,ios::beg);
	disk.write((unsigned char*)&curdir,sizeof(curdir));//ͬ����Ŀ¼��������
	curpath[0]='/';//��ʼ����ǰ·��Ϊ��Ŀ¼'/'
	curpath[1]='\0';
}

void showdir(int dirblock)
{//��ʾָ��Ŀ¼������
	disk.seekg((dirblock+DATABEG)*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)temp,sizeof(temp));//��ȡָ���̿��µ�Ŀ¼
	cout<<"Name     Extension Position Size  CreateTime"<<endl;
	cout.setf(ios::left);
	for(int i=0;i<(temp[0].filesize+2);i++){
		if(strlen(temp[i].name)>8){//���ļ���Ϊ8���ַ�ʱû���ַ���������������д���
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
{//��ʼ��SDisk.dat�����ļ�ϵͳ
	if(init_disk()){//����Ϊ����
		disktomem();//�Ӵ�����ͬ����Ϣ���ڴ�
		curblock=0;//��ʼ����ǰĿ¼��·��
		curpath[0]='/';
		curpath[1]='\0';
	}
	else{//�����ʼ��������Ϣ��fat��map����Ŀ¼
		init_diskinfo();
		init_fat();
		init_map();
		init_root();
		memtodisk();//��ͬ����������
	}
}

unsigned short int allocate(unsigned int size)
{//����size���̿飬�޸�fat��map��������Ϣ������size���̿��еĿ�ʼ�̿��
	unsigned short int beginblock,lastblock;
	if(size<=0){//������Ҫ����Ŀռ�������0
		cout<<"The space apply for must be positive!"<<endl;
		return 0;
	}
	if(dinfo.remainblock < size){//û���㹻�Ŀռ�
		cout<<"No enough space!"<<endl;
		return 0;
	}
	for(unsigned int i=0,j=0;i<DATASIZE&&j<size;i++){//����λʾͼ
		if(map[i/16]&(1<<(15-i%16)))//�̿�����
			continue;
		else{//�ҵ������̿�
			map[i/16]|=(1<<(15-i%16));//map��Ӧλ��1
			if((j++)==0)//��ǰ������ǵ�һ���̿�
				beginblock=i;//�ÿ�ʼ�̿�Ϊi
			else
				fat1[lastblock]=fat2[lastblock]=i;//�������ϴη�����̿��Ӧ��fat��Ϊi
			lastblock=i;//��¼��ǰ�̿��Ա������¸��̿���
		}
	}
	fat1[lastblock]=fat2[lastblock]=0xFF0F;//���һ���̿����ļ�������
	dinfo.usedblock+=size;//���´�����Ϣ
	dinfo.remainblock-=size;
	dinfo.usedspace+=size*BLOCKSIZE;
	dinfo.remainspace-=size*BLOCKSIZE;
	memtodisk();//ͬ��������
	return beginblock;//���ط����̿�Ŀ�ʼ�̿�
}

bool analysisname(char *filename,char *name,char *type)
{//���ַ����ļ����ֽ���ļ�������չ��
	unsigned int i;
	for(i=0;(filename[i]!='.')&&(i<strlen(filename));i++)
		;//��λ���ļ�������չ���ָ���
	if(i>8){//�ļ������ܶ���8λ
		cout<<"ERROR!Length of file name must less than 8!"<<endl;
		return false;
	}
	if(filename[i]=='.'&&(strlen(&filename[i+1])>3)){//��չ�����ܶ���3λ
		cout<<"ERROR!Length of file extension must less than 3!"<<endl;
		return false;
	}
	if(i==strlen(filename)){//����չ��
		strcpy(name,filename);
		type[0]='\0';
	}
	else{//����չ��
		strncpy(name,filename,i);
		name[i]='\0';
		strcpy(type,&filename[i+1]);
		type[strlen(&filename[i+1])]='\0';
	}
	return true;
}

bool isName(bool isdir,char *filename)
{//�ļ�����Ŀ¼�����Ƿ��зǷ��ַ�
	unsigned i,j;
	for(i=0;(filename[i]!='.')&&(i<strlen(filename));i++){//�ҵ��ļ�������չ���ָ���'.'
		if(filename[i]=='/')//�ļ�������Ŀ¼������Ŀ¼��ָ���/
			return false;
	}
	if(isdir&&i!=strlen(filename))//Ŀ¼������'.'
		return false;
	for(j=i+1;j<strlen(filename);j++){
		if(filename[j]=='/'||filename[j]=='.')//�ļ���չ�����зǷ��ַ�
			return false;
	}
	return true;
}

void recycle(int dirblock,int pos)
{//����ָ���Ŀռ䣬�ݹ�
	int size=0;//��¼���յ��̿���
	disk.seekg((dirblock+DATABEG)*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)temp,sizeof(temp));
	directory tempdir;
	memcpy(&tempdir,&temp[pos],sizeof(directory));//�ݴ���Ҫɾ����Ŀ¼��
	temp[pos].name[0]=(char)0xE5;//���Ŀ¼��
	temp[0].filesize--;//��Ŀ¼��������
	if(pos!=temp[0].filesize+2)//����Ŀ¼�������
		memcpy(&temp[pos],&temp[pos+1],(temp[0].filesize+2-pos)*sizeof(directory));
	disk.seekp((dirblock+DATABEG)*BLOCKSIZE,ios::beg);//���µ�������
	disk.write((unsigned char*)temp,sizeof(temp));
	if(tempdir.isdir){//ɾ����Ŀ¼�µ�����
		size+=1;//����Ŀ¼�̿�+1
		map[tempdir.beginblock/16]&=(~(1<<(15-tempdir.beginblock%16)));//��λʾͼ��Ӧλ��
		fat1[tempdir.beginblock]=fat2[tempdir.beginblock]=0;//��fat��Ӧλ��
		disk.seekg((tempdir.beginblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)temp,sizeof(temp));//��ȡ��Ŀ¼��Ŀ¼��		
		for(int i=temp[0].filesize+1;i>1;i--)
			recycle(temp[0].beginblock,i);//�ݹ����Ŀ¼�µ�����
	}
	else{//�����ļ�
		size+=temp[pos].filesize;//�����ļ������մ�С���ļ���С
		int thisblock=tempdir.beginblock,nextblock;
		for(int i=0;i<tempdir.filesize;i++){//��fatָʾ�����̿�
			nextblock=fat1[thisblock];//��¼��һ��Ҫ���յ��̿�
			map[thisblock/16]&=(~(1<<(15-thisblock%16)));//��mapλ
			fat1[thisblock]=fat2[thisblock]=0;//��fatλ
			thisblock=nextblock;
		}
	}
	dinfo.usedblock-=size;//���´�����Ϣ
	dinfo.remainblock+=size;
	dinfo.usedspace-=size*BLOCKSIZE;
	dinfo.remainspace+=size*BLOCKSIZE;
	memtodisk();//ͬ����������
}

int find(char *filename,int block)
{//�����ļ�����block��ָĿ¼�̿����ҵ��ļ�Ŀ¼��
	disk.seekg((block+DATABEG)*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)temp,sizeof(temp));
	if(strcmp(filename,"..")==0)//Ϊ��Ŀ¼
		return 1;
	if(strcmp(filename,".")==0)//Ϊ��Ŀ¼
		return 0;
	for(int i=0;i<temp[0].filesize+2;i++){
		if(temp[i].isdir&&(strcmp(filename,temp[i].name)==0))//Ŀ¼ֱ�ӱȽ�Ŀ¼��
			return i;
		else{//��Ϊ�ļ����ļ�����չ���ϲ��Ƚ�
			char tempname[15];
			if(strlen(temp[i].name)>8){//�ļ���Ϊ8λʱ�����⴦��
				strncpy(tempname,temp[i].name,8);
				tempname[8]='\0';
			}
			else
				strcpy(tempname,temp[i].name);
			if(strlen(temp[i].type)!=0){//�ļ�����չ��
				strcpy(&tempname[strlen(tempname)+1],temp[i].type);
				tempname[strlen(tempname)]='.';
			}
			if(strcmp(tempname,filename)==0)//�Ƚ��ļ���
				return i;
		}			
	}
	return -1;
}

void createdir(char* name)
{//����Ŀ¼
	if(curdir[0].filesize==30){//Ŀ¼��Ŀ¼������
		cout<<"The current directory already has 30 directories!Cannot create directory or file anymore!!"<<endl;
		return;
	}
	if(strlen(name)>8){//���ִ���8λ
		cout<<"The directory name must not bigger than 8 letters!"<<endl;
		return;
	}
	if(!isName(true,name)){//Ŀ¼�����Ƿ��зǷ��ַ�
		cout<<"The directory name has illagel character."<<endl;
		return;
	}
	if(find(name,curblock)!=-1){//�ڵ�ǰĿ¼�����������ļ���Ŀ¼
		cout<<"The directory:"<<name<<" is existing."<<endl;
		return;
	}
	int beginblock=allocate(1);//�����ļ��ռ�
	if(!beginblock){//����ʧ�ܣ��˳�
		cout<<"Failed to create new directory!"<<endl;
		return;
	}
	int pos=(curdir[0].filesize++)+2;
	curdir[pos].beginblock=beginblock;
	strcpy(curdir[pos].name,name);
	init_dir(&curdir[pos]);//��ʼ��Ŀ¼��
	disk.seekp((curblock+DATABEG)*BLOCKSIZE);
	disk.write((unsigned char*)curdir,sizeof(curdir));//ͬ��Ŀ¼������
	memcpy(&temp[0],&curdir[curdir[0].filesize+1],sizeof(directory));//�½�Ŀ¼��һ���ʼ��
	memcpy(&temp[1],&curdir[0],sizeof(directory));//���½�Ŀ¼�ڶ����ʼ��
	temp[1].filesize=0;//��Ŀ¼�޷���֪��Ŀ¼�µ�Ŀ¼�����
	strcpy(temp[0].name,right);
	strcpy(temp[1].name,parent);
	for(int i=2;i<32;i++)//��ʼ���½�Ŀ¼
		temp[i].name[0]=(char)0xE5;
	disk.seekp((curdir[pos].beginblock+DATABEG)*BLOCKSIZE,ios::beg);
	disk.write((unsigned char*)temp,sizeof(temp));//ͬ���½�Ŀ¼������
	disk.seekg((curdir[pos].beginblock+DATABEG)*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)temp,sizeof(temp));//ͬ���½�Ŀ¼������
}

void rmdir(int pos)
{//ɾ��Ŀ¼
	if(curdir[pos].isdir){//ɾ��Ŀ¼��ΪĿ¼
		recycle(curblock,pos);//����Ŀ¼
		disk.seekg((curblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)curdir,sizeof(curdir));//�����ڴ�ͬ��
	}
	else//Ŀ¼�ΪĿ¼������
		cout<<curdir[pos].name<<"is not a directory."<<endl;
}



void createfile(char* filename,char *type,int size)
{//�����ļ�
	if(curdir[0].filesize==30){//Ŀ¼��Ŀ¼������
		cout<<"The current directory already has 30 directories!Cannot create directory or file anymore!!"<<endl;
		return;
	}
	int beginblock=allocate(size);//�����ļ��ռ�
	if(!beginblock){//����ʧ�ܣ��˳�
		cout<<"Failed to create new file!"<<endl;
		return;
	}
	int pos=(curdir[0].filesize++)+2;
	curdir[pos].beginblock=beginblock;//��ʼ���ļ���Ŀ¼��
	strcpy(curdir[pos].name,filename);
	strcpy(curdir[pos].type,type);
	curdir[pos].isdir=false;
	curdir[pos].filesize=size;
	init_dirtime(&curdir[pos]);
	disk.seekp((curblock+DATABEG)*BLOCKSIZE,ios::beg);
	disk.write((unsigned char*)curdir,sizeof(curdir));//ͬ��Ŀ¼������
}

void delfile(int pos)
{//ɾ���ļ�
	if(curdir[pos].isdir)//ɾ����Ŀ¼���ΪĿ¼
		cout<<curdir[pos].name<<" is not a file."<<endl;
	else{//ɾ����Ŀ¼�����ļ�
		recycle(curblock,pos);//�����ļ���ռ�ռ�
		disk.seekg((curblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)curdir,sizeof(curdir));//�������ڴ�ͬ��
	}
}

bool analysispath(char *path,int *block,int *pos)
{//�����ַ�����ʽ·��������·��ָ���Ŀ¼����̿��λ��
	char filename[15];
	int i;
	int len=strlen(path);
	*pos=0;//��ָ·��ָ���ļ�ʱ����posΪ��0ֵ
	if(path[0]=='/'){//����·��
		*block=0;
		if(strlen(path)==1)//�Ǹ�Ŀ¼
			return true;
		else{//����·��
			if(path[1]!='/'&&analysispath(&path[1],block,pos))
				return true;
		}
		return false;
	}
	else{//���·��
		for(i=0;i<len&&path[i]!='/';i++)
			;//��ȡ��ҪѰ�ҵı���Ŀ¼��Ŀ¼�������䳤��
		strncpy(filename,path,i);
		filename[i]='\0';	
		*pos=find(filename,*block);//��Ŀ¼�̿����ҵ�Ŀ¼��
		if(*pos==-1)//�Ҳ����ļ�
			return false;
	}
	if(i==len||i==len-1)//·���������
		return true;
	else{//·��δ������
		if(!temp[*pos].isdir){//��ʱ�ҵ���Ŀ¼�����ļ�������
			cout<<"File:"<<filename<<" is not a directory!"<<endl;
			return false;
		}
		if(path[i+1]=='/'){//·�����ظ���Ŀ¼��ָ���'/'
			cout<<"The path is illegal."<<endl;
			return false;
		}
		*block=temp[*pos].beginblock;//���¼�Ŀ¼��ʼ����·��
		if(analysispath(&path[i+1],block,pos))//�ݹ��������·��
			return true;
		else
			return false;
	}
}

bool isdir(int block,int pos)
{//�ж�Ŀ¼���Ƿ�ΪĿ¼
	disk.seekg((block+DATABEG)*BLOCKSIZE,ios::beg);
	disk.read((unsigned char*)temp,sizeof(temp));
	if(temp[pos].isdir)//·����ָΪĿ¼,����Ϊ�ļ�
		return true;
	else
		return false;
}

void getcurpath()
{//��ȡ��ǰ·��
	strcpy(curpath,"/");//��Ŀ¼
	curpath[1]='\0';
	if(curblock==0)//��ǰΪ��Ŀ¼
		return;
	char path[30][10];
	int i,j,block;
	memcpy(temp,curdir,sizeof(temp));
	for(i=0;temp[0].beginblock!=0;i++){//��ȡ��ǰ·���и�Ŀ¼���Ŀ¼��
		block=temp[0].beginblock;
		disk.seekg((temp[1].beginblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)temp,sizeof(temp));
		for(j=2;j<temp[0].filesize+2;j++){
			if(block==temp[j].beginblock){//�ҵ�Ŀ¼�����Ƶ��ݴ��·��������
				strcpy(path[i],temp[j].name);
			}
		}
	}
	for(j=i;j>0;j--){//��·�������е�Ŀ¼����֯�ɵ�ǰ·��
		strcat(curpath,path[j-1]);
		strcat(curpath,"/");
	}
}

void changedir(char* path)
{//�ı䵱ǰĿ¼�����µ�ǰ·��
	int block,pos=0;
	block=curblock;
	if(analysispath(path,&block,&pos)){//·������
		disk.seekg((block+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)temp,sizeof(temp));
		if(temp[pos].isdir){//·����ָΪĿ¼,����Ϊ�ļ�
			curblock=temp[pos].beginblock;//���õ�ǰĿ¼��ص�ȫ�ֱ���
			disk.seekg((curblock+DATABEG)*BLOCKSIZE,ios::beg);
			disk.read((unsigned char*)curdir,sizeof(curdir));
			getcurpath();//��ȡ��ǰ·��
			return;
		}
	}
	cout<<"Path:"<<path<<" is not exist or not a directory!"<<endl;
}


void copyfile(char* src,char *den)
{//�����ļ�
	int srcblock,srcpos=0;
	int denblock,denpos=0;
	srcblock=curblock;
	denblock=curblock;
	directory srcdir;
	if(analysispath(src,&srcblock,&srcpos)){//Դ�ļ�����
		disk.seekg((srcblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)temp,sizeof(temp));
		if(temp[srcpos].isdir){//ֻ�ܸ����ļ�
			cout<<"Cannot copy directory."<<endl;
			return;
		}
		disk.seekg((srcblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)temp,sizeof(temp));
		memcpy(&srcdir,&temp[srcpos],sizeof(directory));//�ݴ��ҵ����ļ�Ŀ¼��
		if(analysispath(den,&denblock,&denpos)){//Ŀ��·������
			disk.seekg((denblock+DATABEG)*BLOCKSIZE,ios::beg);
			disk.read((unsigned char*)temp,sizeof(temp));
			if(!temp[denpos].isdir){//Ŀ��·������ָ��Ŀ¼
				cout<<"The destinate path shouldnot be a file."<<endl;
				return;
			}
			srcblock=curblock;
			disk.seekg((temp[denpos].beginblock+DATABEG)*BLOCKSIZE,ios::beg);
			curblock=temp[denpos].beginblock;
			memcpy(temp,curdir,sizeof(curdir));//���浱ǰĿ¼�Ա���ԭ
			disk.read((unsigned char*)curdir,sizeof(curdir));
			for(int i=2;i<curdir[0].filesize+2;i++){//Ŀ��·�����������ļ�
				if((strcmp(srcdir.name,temp[i].name)==0)&&(strcmp(srcdir.type,temp[i].type)==0)){
					cout<<"The file is existing in the destinate directory."<<endl;
					return;
				}
			}
			createfile(srcdir.name,srcdir.type,srcdir.filesize);//�����ļ�
			curblock=srcblock;//��ԭ��ǰĿ¼
			memcpy(curdir,temp,sizeof(curdir));
			return;
		}
		else//Ŀ��·��������
			cout<<"Destination path:"<<den<<" is not exist!"<<endl;
	}
	else//Դ�ļ�������
		cout<<"Source path:"<<src<<" is not exist!"<<endl;
}

void rename(char *oldname,char *newname)
{//�������ļ�
	int pos=find(oldname,curblock);//�ҵ��ļ�
	if(pos==-1)
		return;
	if(analysisname(newname,curdir[pos].name,curdir[pos].type)){
		//ͬ�������̲����¶�ȡ
		disk.seekp((curblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.write((unsigned char*)curdir,sizeof(curdir));
		disk.seekg((curblock+DATABEG)*BLOCKSIZE,ios::beg);
		disk.read((unsigned char*)curdir,sizeof(curdir));
	}
	else
		cout<<"Failed to rename the file:"<<oldname<<endl;
}

void showhelpinfo()
{//��ʾ�ļ�ϵͳ�İ�����Ϣ
	cout<< "==================================================\n";
	cout<< "              FAT�ļ�ϵͳ����˵�             \n";		
	cout<< "   dir                         ��ʾĿ¼����\n";
	cout<< "   cd [Path]                   Ŀ¼�л�\n";
	cout<< "   mkdir [Dirname]             ����Ŀ¼\n";
	cout<< "   rmdir [Dirname]             ɾ��Ŀ¼\n";
	cout<< "   mkfile [Filename][Size]     �����ļ�\n";
	cout<< "   rename [oldname] [newname]  �ļ���Ŀ¼������\n";
	cout<< "   cpfile [FilePath] [DirPath] �����ļ�\n";
	cout<< "   del [Filename]|[Path]       ɾ���ļ�\n";
	cout<< "   showdisk                    ��ʾ������Ϣ\n";
	cout<< "   showfat                     ��ʾFAT\n";
	cout<< "   showmap                     ��ʾλʾͼmap\n";
	cout<< "   format                      ��ʽ������\n";
	cout<< "   clear                       ����\n";
	cout<< "   help                        ��ʾ����\n";
	cout<< "   exit                        �˳�\n";
	cout<< "=================================================\n";
}

bool getpara(int num)
{//��ȡ����
	int i,j,k;
	char temp[100];
	cin.getline(temp,99);
	int length=strlen(temp);
	bool pa=false;
	for(i=0,j=0;i<length;i++){//�����ȡ�Ĳ�������
		if(temp[i]==' ')
			pa=true;
		if(temp[i]!=' '&&pa){
			j++;
			pa=false;
		}
	}
	if(j!=num){//������������
		cout<<"Command '"<<cmd<<"' doesn't take "<<j<<" parameters!"<<endl;
		cout<<"Please enter'help' to get help information."<<endl;
		return false;
	}
	for(i=0,j=0,k=0;i<length;i++){//���Ʋ����������б�
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
{//��ȡ�û���������ִ����Ӧ����
	bool exit=false;
	while(!exit){
		cout<<"[Path: "<<curpath<<" ]# ";//�����ǰ·���͵ȴ������������#
		cin>>cmd;//��ȡ����
		if(strlen(cmd)==0)//�����Ϊ��
			continue;
		if(strcmp(cmd,"dir")==0){//����Ϊdir
			if(getpara(0))
				showdir(curblock);//��ʾ��ǰĿ¼��Ϣ
		}
		else if(strcmp(cmd,"cd")==0){//����Ϊcd
			if(getpara(1))
				changedir(parameter[0]);//�ı䵱ǰĿ¼
		}
		else if(strcmp(cmd,"mkdir")==0){//����Ϊmkdir
			if(getpara(1))
				createdir(parameter[0]);//�ڵ�ǰĿ¼�´���Ŀ¼
		}
		else if(strcmp(cmd,"rmdir")==0){//����Ϊrmdir
			if(getpara(1)){
				int pos;
				if(strcmp(parameter[0],"..")==0)//����ɾ����Ŀ¼
					cout<<"Cannot remove the parent directory."<<endl;
				else if(strcmp(parameter[0],".")==0)//����ɾ����Ŀ¼
					cout<<"Cannot remove the current directory."<<endl;
				else{
					pos=find(parameter[0],curblock);
					if(pos==-1)//��Ŀ¼������
						cout<<"The directory:'"<<parameter[0]<<"' is not exist in the current directory!"<<endl;
					else
						rmdir(pos);//��Ŀ¼���ڣ�ɾ����Ŀ¼
				}
			}
		}
		else if(strcmp(cmd,"mkfile")==0){//����Ϊmkfile
			if(getpara(2)){
				char name[10],type[5];
				if(isName(false,parameter[0])){//�ļ��������Ƿ��ַ�
					if(find(parameter[0],curblock)==-1){//��ǰĿ¼û�������ļ�
						if(analysisname(parameter[0],name,type))
							createfile(name,type,atoi(parameter[1]));//�ڵ�ǰĿ¼�´������ļ�
					}
					else
						cout<<"The file:"<<parameter[0]<<" is existing."<<endl;
				}
				else
					cout<<"The path:"<<parameter[0]<<" has illagel character."<<endl;
			}
		}
		else if(strcmp(cmd,"rename")==0){//����Ϊrename
			if(getpara(2))
				rename(parameter[0],parameter[1]);//�������ļ�
		}
		else if(strcmp(cmd,"cpfile")==0){//����Ϊcpfile
			if(getpara(2))
				copyfile(parameter[0],parameter[1]);//�����ļ�
		}
		else if(strcmp(cmd,"del")==0){//����Ϊdel
			if(getpara(1)){
				int pos;
				pos=find(parameter[0],curblock);
				if(pos==-1)//��ǰĿ¼��û�и��ļ�
					cout<<"File:"<<parameter[0]<<" is not exist in the current directory."<<endl;
				else
					delfile(pos);//�ļ����ڣ�ɾ���ļ�
			}
		}
		else if(strcmp(cmd,"showdisk")==0){//����Ϊshowdisk
			if(getpara(0))
				showdiskinfo();//��ʾ������Ϣ
		}
		else if(strcmp(cmd,"showfat")==0){//����Ϊshowfat
			if(getpara(0))
				showfat();//��ʾFAT��
		}
		else if(strcmp(cmd,"showmap")==0){//����Ϊshowmap
			if(getpara(0))
				showmap();//��ʾλʾͼ
		}
		else if(strcmp(cmd,"format")==0){//����Ϊformat
			if(getpara(0)){
format:			cout<<"Format disk will lost all file! Please make sure.[Y/N]:";
				char ch;
				cin>>ch;
				cin.ignore(1000,'\n');
				if(ch=='Y'||ch=='y'){//��ʽ������
					disk.close();//�ر��ļ���
					remove(diskpath);//ɾ���ļ�
					init_fs();//��ʼ���´���
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
		else if(strcmp(cmd,"clear")==0){//����Ϊclear
			if(getpara(0))
				system("cls");//����
		}
		else if(strcmp(cmd,"help")==0){//����Ϊhelp
			if(getpara(0))
				showhelpinfo();//��ʾ������Ϣ
		}
		else if(strcmp(cmd,"exit")==0){//����Ϊexit
			if(getpara(0)){
				disk.close();//�رմ���
				exit=true;
			}
		}
		else{//�������
			cout<<"'"<<cmd<<"' is not a valid commmand."<<endl;
			cin.ignore(1000,'\n');
		}
		if(exit==true)//�ж��Ƿ��˳�����
			cout<<"Exit the filesystem......"<<endl;
		cmd[0]=parameter[0][0]=parameter[1][0]='\0';//��������ַ����Ͳ����б�
	}
}

void main()
{//������
	init_fs();//��ʼ��������̺��ļ�ϵͳ
	get_command();//�ȴ��û���������
}
