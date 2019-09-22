#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BF.h"
#include "HT.h"

#define FILENAME "file"
#define MAX_FILES 1
#define MAX_BLOCKS 5

int main(int argc, char** argv) {
	int i,j,block_id,del_value,test_value,ret_val;
	void *block_ptr;
	Block *block;
	HT_info *ht_info;
	SHT_info *sht_info;
	Record *myrec,test_rec;
	SecondaryRecord sec_rec;
	char name[10],c,buffer[4][100];
	char test_string[100];
	FILE *fp;
	char* fileName="primary.index";
	char* sfileName="secondary.index";

	if (argc < 3) { //check if 2 file names were given in command line
		printf("usage:%s <file1> <file2> (not the same)\n",argv[0]);
		return -1;
	}

	BF_Init();
	//create a primary file with id as primary key
	HT_CreateIndex(fileName,'i',"id",strlen("id"),MAX_BUCKETS); 
	//open the primary index file
	ht_info = HT_OpenIndex("primary.index");
	//open the first file that was given in the command line
	if ((fp = fopen(argv[1],"r")) == NULL){printf("Error! opening file");exit(1);}
	//Read line by line records from the file and insert them to the primary index file
	i = 0; j = 0;
	while (1) {
		c = getc(fp);
		if (c != '{' && c != '}' && c != ',' && c != '\n' && c!= '"' && c!= EOF) {
			buffer[i][j++] = c;
		}
		else if (c == ',' || c == '}') {
			buffer[i][j] = '\0';
			j = 0;
			i++;
		}
		else if (c == '\n' || c == EOF) {
			test_rec.id =atoi(buffer[0]);
			strcpy(test_rec.name,buffer[1]);
			strcpy(test_rec.surname,buffer[2]);
			strcpy(test_rec.address,buffer[3]);
			HT_InsertEntry(*ht_info,test_rec);
			i = 0;
			j = 0;
			printf("{%d,%s,%s,%s} was inserted\n",test_rec.id,test_rec.name,test_rec.surname,test_rec.address);
		}
		if (c == EOF) {
			break;
		}
	}
	//search an entry with a specific id
	printf("Give an id for searching:");
	scanf("%d",&test_value);
	ret_val = HT_GetAllEntries(*ht_info,(void*)&test_value);
	if (ret_val == -1) {
		printf("Entry with id %d doesn't exist\n",test_value);
	}
	else {
		printf("Entry with id %d exist\n",test_value);
	}
	//delete an entry with a specific id
	printf("Give an id for deletion:");
	scanf("%d",&test_value);
	HT_DeleteEntry(*ht_info,(void*)&test_value);
	printf("Entry with id %d got just deleted\n",test_value);
	ret_val = HT_GetAllEntries(*ht_info,(void*)&test_value);
	if (ret_val == -1) {
		printf("Entry with id %d doesn't exist\n",test_value);
	}
	else {
		printf("Entry with id %d exist\n",test_value);
	}

	char sAttrType='c';
	char* sAttrName="name";
	int sAttrLength=15;
	int sBuckets=MAX_BUCKETS;
	//create a secondary index file
	SHT_CreateSecondaryIndex(sfileName,sAttrName,sAttrLength,sBuckets,fileName);
	//open it
	sht_info = SHT_OpenSecondaryIndex(sfileName);

	//open the second file that was given in the command line
	if ((fp = fopen(argv[2],"r")) == NULL){printf("Error! opening file");exit(1);}
	//Read line by line records from the file and insert them to the primary and secondary
	//index files
	i = 0; j = 0;
	while (1) {
		c = getc(fp);
		if (c != '{' && c != '}' && c != ',' && c != '\n' && c!= '"' && c!= EOF) {
			buffer[i][j++] = c;
		}
		else if (c == ',' || c == '}') {
			buffer[i][j] = '\0';
			j = 0;
			i++;
		}
		else if (c == '\n' || c == EOF) {
			//printf("%d %s %s %s\n",atoi(buffer[0]),buffer[1],buffer[2],buffer[3]);
			test_rec.id =atoi(buffer[0]);
			strcpy(test_rec.name,buffer[1]);
			strcpy(test_rec.surname,buffer[2]);
			strcpy(test_rec.address,buffer[3]);
			//insert that record to the primary index file
			//and keep the block id that the record was saved
			int blockid = HT_InsertEntry(*ht_info,test_rec);

			sec_rec.record = test_rec;
			sec_rec.blockId = blockid; //block id to the primary index file
			//insert that record to the primary index file
			SHT_SecondaryInsertEntry(*sht_info,sec_rec);
			i = 0;
			j = 0;
			printf("{%d,%s,%s,%s} was inserted in primary and secondary index file\n",
				test_rec.id,test_rec.name,test_rec.surname,test_rec.address);
		}
		if (c == EOF) {
			break;
		}
	}
	//search an entry with a specific name to the secondary index file
	printf("Give an id for searching in secondary index file:");
	scanf("%s",test_string);
	ret_val = SHT_SecondaryGetAllEntries(*sht_info,*ht_info,(void*)test_string);
	if (ret_val == -1) {
		printf("Entry with name %s doesn't exist\n",test_string);
	}
	else {
		printf("Entry with name %s exist\n",test_string);
	}
	//close primary index file
	HT_CloseIndex(ht_info);
	//close secondary index file
	SHT_CloseSecondaryIndex(sht_info);
	fclose(fp);

	printf("Statistics of primary file\n");
	HashStatistics(fileName);
	printf("Statistics of secondary file\n");
	HashStatistics(sfileName);
	return 0;
}

