#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "BF.h"
#include "HT.h"

int global_fd;

int hash_int(int x) {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return x%(MAX_BUCKETS);
}

int hash_string(char *str) { //djb2 hash function
	unsigned long hash = 5381;
	int c;
	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash%(MAX_BUCKETS);
}

int HT_CreateIndex(char *fileName,char attrType,char* attrName,int attrLength,int buckets) {
	int fileDesc,blkCnt,i,j;
	void *block;
	HT_info ht_info,*temp;
	Block ablock;

	if (BF_CreateFile(fileName) < 0) {BF_PrintError("Error creating file");exit(EXIT_FAILURE);}
	
	if ((fileDesc = BF_OpenFile(fileName)) < 0) {BF_PrintError("Error opening file");return -1;}
	//initialize the header struct that we will write to the disk
	ht_info.fileDesc = fileDesc;
	ht_info.attrType = attrType;
	ht_info.attrLength = attrLength;
	ht_info.attrName = (char*)malloc(sizeof(char)*attrLength);
	strcpy(ht_info.attrName,attrName);
	ht_info.numBuckets = buckets;

	for(i=0; i<MAX_BUCKETS+1; i++) {
		if (BF_AllocateBlock(fileDesc) < 0) {BF_PrintError("Error allocating block");return -1;}
		if (BF_ReadBlock(fileDesc, i , &block) < 0) {BF_PrintError("Error getting block");return -1;}
		if (i == 0) { //in block ,we store information about the index file
			for(j=0; j<MAX_BUCKETS; j++) {
				ht_info.buckets[j] = j+1; //a bucket array
			}
			memcpy(block,&ht_info,sizeof(ht_info)); //copy the ht_info struct
			memcpy(block+sizeof(ht_info),ht_info.attrName,sizeof(char)*attrLength);
			//write it to the disk
			if (BF_WriteBlock(fileDesc, 0) < 0){BF_PrintError("Error writing block back");return -1;}
		}
		else {
			//initialize every bucket
			ablock.recordsCounter = 0;
			ablock.next_block = -1;
			for(j=0; j<MAX_RECORDS; j++) {
				ablock.bit_array[j] = 0; //empty block with no records
			}
			memcpy(block,&ablock,sizeof(Block));
			if (BF_WriteBlock(fileDesc, i) < 0){BF_PrintError("Error writing block back");return -1;}
		}
	}

	if (BF_CloseFile(fileDesc) < 0) {BF_PrintError("Error closing file");return -1;}
	free(ht_info.attrName);
	return 0;
}

HT_info* HT_OpenIndex(char *fileName) {
	HT_info *ht_info;
	int fileDesc;
	void *block;
	if ((fileDesc = BF_OpenFile(fileName)) < 0) {BF_PrintError("Error opening file");return NULL;}
	
	if (BF_ReadBlock(fileDesc, 0 , &block) < 0) {BF_PrintError("Error getting block");return NULL;}

	ht_info = (HT_info*)malloc(sizeof(HT_info));
	memcpy(ht_info,block,sizeof(HT_info));
	ht_info->attrName = (char*)malloc(sizeof(char)*ht_info->attrLength);
	memcpy(ht_info->attrName,block+sizeof(HT_info),sizeof(char)*ht_info->attrLength);

	global_fd = fileDesc;
	return ht_info;
}

int HT_CloseIndex(HT_info* header_info) {
	if (BF_CloseFile(header_info->fileDesc) < 0) {BF_PrintError("Error closing file");return -1;}
	free(header_info->attrName);
	free(header_info);
	return 0;
}

int HT_InsertEntry(HT_info header_info,Record record) {
	void *block_ptr;
	Block *block;
	int position,fileDesc,i,j,blockid;

	if (strcmp(header_info.attrName,"id") == 0){
		position = hash_int(record.id);
	}
	else if (strcmp(header_info.attrName,"name") == 0) {
		position = hash_string(record.name);
	}
	else if (strcmp(header_info.attrName,"surname") == 0) {
		position = hash_string(record.surname);
	}
	else {
		position = hash_string(record.address);
	}
	fileDesc = header_info.fileDesc;
	
	if (BF_ReadBlock(fileDesc,header_info.buckets[position], &block_ptr) < 0) {
			BF_PrintError("Error getting block");return -1;}
	block = (Block*)block_ptr;
	blockid = header_info.buckets[position];

	while(1) { //find a block with free space for insertion
		if (block->recordsCounter >= MAX_RECORDS) {
			//traverse the overflow block list until one with free space is found
			if (block->next_block != -1) {
				blockid = block->next_block;
				if (BF_ReadBlock(fileDesc,block->next_block, &block_ptr) < 0) {
					BF_PrintError("Error getting block");return -1;}
				block = (Block*)block_ptr;
				continue;
			}
			else { //allocate a new block if there is no block with free space
				if (BF_AllocateBlock(fileDesc) < 0) 
					{BF_PrintError("Error allocating block");return -1;}
				//make the previous block point to the new one
				block->next_block = BF_GetBlockCounter(fileDesc) - 1; 
				//write that block to disk
				if (BF_WriteBlock(fileDesc, blockid) < 0){BF_PrintError("Error writing block back");return -1;}
				//keep the blockid of the new allocated block
				blockid = block->next_block;
				//read the new allocated block
				if (BF_ReadBlock(fileDesc,BF_GetBlockCounter(fileDesc)-1, &block_ptr) < 0) 
					{BF_PrintError("Error getting block");return -1;}
				block = (Block*)block_ptr;
				//initialization of the new allocated block
				block->recordsCounter = 0;
				block->next_block = -1;
				for(j=0; j<MAX_RECORDS; j++) {
					block->bit_array[j] = 0;
				}
			}
		}
		break;
	}
	//insert that entry
	for(i=0; i<MAX_RECORDS; i++) {
		if (block->bit_array[i] == 0) {
			block->records[i] = record;
			block->bit_array[i] = 1;
			block->recordsCounter++;
			if (BF_WriteBlock(fileDesc, blockid) < 0){BF_PrintError("Error writing block back");return -1;}
			return blockid;
		}
	}
	return -1;
}

int HT_DeleteEntry(HT_info header_info,void *value) {
	int i,j,position,fileDesc,blockid,found;
	void *block_ptr;
	Block *block;

	if (strcmp(header_info.attrName,"id") == 0){
		position = hash_int(*(int*)value);
	}
	else {
		position = hash_string((char*)value);
	}
	fileDesc = header_info.fileDesc;
	blockid = header_info.buckets[position];

	found = 0;
	while(1) {
		if (BF_ReadBlock(fileDesc,blockid, &block_ptr) < 0) {
			BF_PrintError("Error getting block");return -1;}
		block = (Block*)block_ptr;
		for(j=0; j<MAX_RECORDS; j++) {
			if (block->bit_array[j] == 1) {

				if (strcmp(header_info.attrName,"id") == 0){
					if (block->records[j].id == *(int*)value) {
						block->bit_array[j] = 0;
						found = 1;
					}
				}
				else if (strcmp(header_info.attrName,"name") == 0) {
					if (strcmp(block->records[j].name,(char*)value) == 0) {
						block->bit_array[j] = 0;
						found = 1;
					}
				}
				else if (strcmp(header_info.attrName,"surname") == 0) {
					if (strcmp(block->records[j].surname,(char*)value) == 0) {
						block->bit_array[j] = 0;
						found = 1;
					}
				}
				else {
					if (strcmp(block->records[j].address,(char*)value) == 0) {
						block->bit_array[j] = 0;
						found = 1;
					}
				}
				//if that record was found,delete it and write it back to the disk
				if (found == 1) {
					if (BF_WriteBlock(fileDesc, blockid) < 0){BF_PrintError("Error writing block back");return -1;}
					return 0;
				}
			}
		}
		//go to the next block
		if (block->next_block != -1) {
			blockid = block->next_block;}
		else {
			break;} 
	}
	return -1;
}

int HT_GetAllEntries(HT_info header_info,void *value) {
	int i,j,position,fileDesc,block_counter,flag,found,blockid;
	void *block_ptr;
	Block *block;

	if (strcmp(header_info.attrName,"id") == 0){
		position = hash_int(*(int*)value);
	}
	else {
		position = hash_string((char*)value);
	}
	fileDesc = header_info.fileDesc;
	blockid = header_info.buckets[position];
	block_counter = 0; flag = 0;
	while(1) {
		if (BF_ReadBlock(fileDesc,blockid, &block_ptr) < 0) {
			BF_PrintError("Error getting block");return -1;}
		block = (Block*)block_ptr;
		block_counter++;
		for(j=0; j<MAX_RECORDS; j++) {
			found = 0;
			if (block->bit_array[j] == 1) {
				if (strcmp(header_info.attrName,"id") == 0){
					if (block->records[j].id == *(int*)value) {
						found = 1;
					}
				}
				else if (strcmp(header_info.attrName,"name") == 0) {
					if (strcmp(block->records[j].name,(char*)value) == 0) {
						found = 1;
					}
				}
				else if (strcmp(header_info.attrName,"surname") == 0) {
					if (strcmp(block->records[j].surname,(char*)value) == 0) {
						found = 1;
					}
				}
				else {
					if (strcmp(block->records[j].address,(char*)value) == 0) {
						found = 1;
					}
				}
				if (found == 1) {
					printf("bucket:%d {%d,%s,%s,%s}\n",position,block->records[j].id,
					  block->records[j].name,block->records[j].surname,block->records[j].address);
					flag = 1;
				}
			}
		}
		if (block->next_block != -1) {
			blockid = block->next_block;}
		else {
			break;} 
	}
	if (flag == 1) 
		{return block_counter;}
	else 
		{return -1;}
}

void HT_PrintEntries(HT_info *ht_info) {
	int i,j,block_id;
	Block *block;
	void *block_ptr;
	for (i=0; i < MAX_BUCKETS; i++) {
		block_id = ht_info->buckets[i];
		while(1) {
			if (BF_ReadBlock(ht_info->fileDesc,block_id, &block_ptr) < 0) {
				BF_PrintError("Error getting block");return;}
			block = (Block*)block_ptr;

			for(j=0; j<MAX_RECORDS; j++) {
				if (block->bit_array[j] == 1) {
					if (j == 0) {printf("bucket = %d\n",i);}
					printf("\t{%d,%s,%s,%s}\n",block->records[j].id,block->records[j].name,
						block->records[j].surname,block->records[j].address);
				}
			}
			if (block->next_block != -1) {
				block_id = block->next_block;}
			else {
				break;} 
		}
	}
}

int HashStatistics(char* fileName) {
	int i,j,block_id,blocks_count=0,min_rec,max_rec=-1,avg_rec,rec_count=0;
	int avg_blocks,over_buckets=0,over_blocks=0,flag = 0,cur_blocks_count=0;
	int cur_rec_count =0;
	int cur_over_blocks=0,fileDesc,p_or_s;
	Block block;
	SecondaryBlock s_block;
	void *block_ptr,*temp;
	HT_info *ht_info;
	SHT_info *sht_info;
	//check if we are dealing with a primary or secondary index  file
	if (strstr(fileName,"primary") != NULL) {
		p_or_s = 1; //boolean variable
	}
	else {	
		p_or_s = 0; //1=primary,0=secondary
	}

	if (p_or_s == 1) { //open the right index file
		ht_info = HT_OpenIndex(fileName);
		fileDesc = ht_info->fileDesc;
	}
	else {
		sht_info = SHT_OpenSecondaryIndex(fileName);
		fileDesc = sht_info->fileDesc;
	}


	min_rec = INT_MAX;
	//for each bucket
	for (i=0; i < MAX_BUCKETS; i++) {
		//read the first block id of this bucket
		if (p_or_s == 1) {
			block_id = ht_info->buckets[i];
		}
		else {
			block_id = sht_info->buckets[i];
		}
		//traverse all blocks
		while(1) {
			blocks_count++; //number of total blocks
			cur_blocks_count++;//number of blocks i current bucket
			//read block with "block_id"
			if (BF_ReadBlock(fileDesc,block_id, &block_ptr) < 0) {	
				BF_PrintError("Error getting block22223");return -1;}
			if (p_or_s == 1) {
				block = *(Block*)block_ptr;
			}
			else {
				s_block = *(SecondaryBlock*)block_ptr;
			}	
			//traverse all records
			for(j=0; j<MAX_RECORDS; j++) {
				if (p_or_s == 1) {
					if (block.bit_array[j] == 1) {
						cur_rec_count++; //records in this block
						rec_count++; //records in this bucket
					}
				}
				else {
					
					if (s_block.bit_array[j] == 1) {
						cur_rec_count++;
						rec_count++;
					}
				}

			}//minimum and maximum records in this bucket
			if (cur_rec_count > max_rec) {max_rec = cur_rec_count;}
			if (cur_rec_count < min_rec) {min_rec = cur_rec_count;}

			cur_rec_count=0;
			if (p_or_s == 1) {
				//if next block exists,go to that block
				if (block.next_block > -1) {
					over_blocks++; //total overflow blocks in the index file
					cur_over_blocks++; //overflow blocks in this bucket
					flag++;
					block_id = block.next_block;
				}
				else {//else break,and go to the next bucket
					break;
				}
			}
			else {
				if (s_block.next_block > -1) {
					over_blocks++;
					cur_over_blocks++;
					flag++;
					block_id = s_block.next_block;
				}
				else {
					break;
				}	
			}


		}
		
		if (flag >= 1) {over_buckets++;}
	
		printf("Overflow blocks of bucket %d is:%d\n",i,cur_over_blocks);
		printf("min records of bucket %d is:%d\n",i,min_rec);
		printf("max records of bucket %d is:%d\n",i,max_rec);
		printf("avg records of bucket %d is:%f\n",i,rec_count/(double)cur_blocks_count);
		//getchar();
		cur_over_blocks = 0;
		cur_blocks_count =  0;
		rec_count = 0;
		max_rec=-1;
		min_rec = INT_MAX; 
		flag = 0;
	}
	printf("\nEvery bucket has %f blocks on average\n",(double)blocks_count/MAX_BUCKETS);
	printf("%d buckets have overflow blocks\n",over_buckets);
	printf("\nFile: %s has %d blocks\n",fileName,blocks_count);
	return 0;
}

SHT_info* SHT_OpenSecondaryIndex( char *sfileName ) {
	SHT_info *sht_info;
	int fileDesc;
	void *block;
	if ((fileDesc = BF_OpenFile(sfileName)) < 0) {BF_PrintError("Error opening file");return NULL;}
	
	if (BF_ReadBlock(fileDesc, 0 , &block) < 0) {BF_PrintError("Error getting block");return NULL;}

	sht_info = (SHT_info*)malloc(sizeof(SHT_info));
	sht_info->fileDesc = fileDesc;
	memcpy(sht_info,block,sizeof(SHT_info));
	sht_info->attrName = (char*)malloc(sizeof(char)*sht_info->attrLength);
	memcpy(sht_info->attrName,block+sizeof(SHT_info),sizeof(char)*sht_info->attrLength);
	sht_info->fileName =(char*)malloc(sht_info->filesize*sizeof(char));
    memcpy(sht_info->fileName, block+sizeof(SHT_info)+((sht_info->attrLength)*sizeof(char)), sht_info->filesize*sizeof(char));

	return sht_info;
}

int SHT_CloseSecondaryIndex( SHT_info *header_info ) {
	if (BF_CloseFile(header_info->fileDesc) < 0) {BF_PrintError("Error closing file");return -1;}
	free(header_info->attrName);
	free(header_info->fileName);
	free(header_info);
	return 0;
}

int SHT_SecondaryInsertEntry(SHT_info header_info,SecondaryRecord record) {
	void *block_ptr;
	SecondaryBlock *block;
	int position,fileDesc,i,j,blockid;
	//choose the right struct field for hashgin
	if (strcmp(header_info.attrName,"id") == 0){
		position = hash_int(record.record.id);
	}
	else if (strcmp(header_info.attrName,"name") == 0) {
		position = hash_string(record.record.name);
	}
	else if (strcmp(header_info.attrName,"surname") == 0) {
		position = hash_string(record.record.surname);
	}
	else {
		position = hash_string(record.record.address);
	}

	fileDesc = header_info.fileDesc;
	
	if (BF_ReadBlock(fileDesc,header_info.buckets[position], &block_ptr) < 0) {
			BF_PrintError("Error getting block");return -1;}
	block = (SecondaryBlock*)block_ptr;
	blockid = header_info.buckets[position];
	while(1) { //find a block with free space for insertion
		//it is full
		if (block->recordsCounter >= MAX_RECORDS) {
			//there are overflow blocks
			if (block->next_block != -1) {
				blockid = block->next_block;
				if (BF_ReadBlock(fileDesc,block->next_block, &block_ptr) < 0) {
					BF_PrintError("Error getting block");return -1;}
				block = (SecondaryBlock*)block_ptr;
				continue;
			}
			//there is not any block with free space
			//so we allocate a new one
			else {
				if (BF_AllocateBlock(fileDesc) < 0) 
					{BF_PrintError("Error allocating block");return -1;}
				//make the previous block point to the new one
				block->next_block = BF_GetBlockCounter(fileDesc) - 1; 
				//write that block to disk
				if (BF_WriteBlock(fileDesc, blockid) < 0){BF_PrintError("Error writing block back");return -1;}
				//keep the blockid of the new allocated block
				blockid = block->next_block;
				//read the new allocated block
				if (BF_ReadBlock(fileDesc,BF_GetBlockCounter(fileDesc)-1, &block_ptr) < 0) 
					{BF_PrintError("Error getting block");return -1;}
				block = (SecondaryBlock*)block_ptr;
				//initialization of the new allocated block
				block->recordsCounter = 0;
				block->next_block = -1;
				for(j=0; j<MAX_RECORDS; j++) {
					block->bit_array[j] = 0;
				}
			}
		}
		break;
	}
	
	for(i=0; i<MAX_RECORDS; i++) {
		if (block->bit_array[i] == 0) {
			block->records[i].id = record.record.id;
			//printf("%s\n",record.record.name);
			//getchar();
			strcpy(block->records[i].name,record.record.name);
			block->records[i].blockId = record.blockId;
			block->bit_array[i] = 1;
			block->recordsCounter++;
			if (BF_WriteBlock(fileDesc, blockid) < 0){BF_PrintError("Error writing block back");return -1;}
			return blockid;
		}
	}
	return -1;
}

int SHT_SecondaryGetAllEntries( SHT_info header_info_sht,HT_info header_info_ht,void *value) {

	int i,j,position,fileDesc,flag,found,blockid,m;
	void *p_block_ptr;
	void *s_block_ptr;
	int block_counter,key;

	SecondaryBlock *s_block;
	Block *p_block;
	position = hash_string((char*)value);

	fileDesc = header_info_sht.fileDesc;
	blockid = header_info_sht.buckets[position];
	block_counter = 0; flag = 0;

	while(1) {
		if (BF_ReadBlock(fileDesc,blockid, &s_block_ptr) < 0) {
			BF_PrintError("Error getting block1");return -1;}
		s_block = (SecondaryBlock*)s_block_ptr;	

		for(j=0; j<MAX_RECORDS; j++) {
			found = 0;
			if (s_block->bit_array[j] == 1) {
				if (strcmp(s_block->records[j].name,(char*)value) == 0) {
					found = 1;
					key = s_block->records[j].id;
				}
				if (found == 1) {
					if (BF_ReadBlock(global_fd,s_block->records[j].blockId, &p_block_ptr) < 0) {
						BF_PrintError("Error getting block2");return -1;}
					p_block = (Block*)p_block_ptr;
					for(m=0; m<MAX_RECORDS; m++) {
						if (p_block->records[m].id == key && p_block->bit_array[m] == 1) {
						printf("\t{%d,%s,%s,%s}\n",p_block->records[m].id,p_block->records[m].name,
							p_block->records[m].surname,p_block->records[m].address);
						}
					}
					flag = 1;
				}
			}
		}
		block_counter++;
		if (s_block->next_block != -1) {
			blockid = s_block->next_block;}
		else {
			break;} 
	}

	if (flag == 1) {
		return block_counter;}
	else 
		{return -1;}
}

int SHT_CreateSecondaryIndex(char *sfileName, char *attrName, int attrLength, int buckets,
	char *fileName) {

	int fileDesc,blkCnt,i,j,block_id,flag;
	int block_id2;
	void *p_block_ptr,*s_block_ptr;
	Block p_block;
	SecondaryBlock ablock,*s_block;
	int k,m,found;
	SHT_info sht_info,*temp;
	HT_info *ht_info;
	

	if (BF_CreateFile(sfileName) < 0) {BF_PrintError("Error creating file");exit(EXIT_FAILURE);}
	
	if ((fileDesc = BF_OpenFile(sfileName)) < 0) {BF_PrintError("Error opening file");return -1;}

	sht_info.fileDesc = fileDesc;
	sht_info.attrLength = attrLength;
	sht_info.numBuckets = buckets;
	sht_info.attrName = (char*)malloc(attrLength*sizeof(char));
	strcpy(sht_info.attrName, attrName);
	sht_info.filesize = strlen(fileName)+1;
	sht_info.fileName = (char*)malloc(sht_info.filesize*sizeof(char));
	strcpy(sht_info.fileName, fileName);
	//create the first on the index file
	//and initialize the bucket array
	for(i=0; i<MAX_BUCKETS+1; i++) {
		if (BF_AllocateBlock(fileDesc) < 0) {BF_PrintError("Error allocating block");return -1;}
		if (BF_ReadBlock(fileDesc, i , &s_block_ptr) < 0) {BF_PrintError("Error getting block");return -1;}
		if (i == 0) {
			for(j=0; j<MAX_BUCKETS; j++) {
				sht_info.buckets[j] = j+1;
			}
			memcpy(s_block_ptr,&sht_info,sizeof(SHT_info));
			memcpy(s_block_ptr+sizeof(SHT_info),sht_info.attrName,sizeof(char)*attrLength);
 	   		memcpy(s_block_ptr+sizeof(SHT_info)+(sizeof(char)*attrLength),sht_info.fileName,(strlen(fileName)+1)*sizeof(char));
    		memcpy(s_block_ptr+sizeof(SHT_info)+(sizeof(char)*attrLength),sht_info.fileName, sizeof(char)*(sht_info.filesize));
			if (BF_WriteBlock(fileDesc, 0) < 0){BF_PrintError("Error writing block back");return -1;}

		}
		else {
			ablock.recordsCounter = 0;
			ablock.next_block = -1;
			for(j=0; j<MAX_RECORDS; j++) {
				ablock.bit_array[j] = 0;
			}
			memcpy(s_block_ptr,&ablock,sizeof(SecondaryBlock));
			if (BF_WriteBlock(fileDesc, i) < 0){BF_PrintError("Error writing block back");return -1;}
		}
	}

	//open primary
	if (BF_ReadBlock(global_fd, 0 , &p_block_ptr) < 0) {BF_PrintError("Error getting block");return -1;}
	//read the primary ht_info struct from the block 0
	ht_info = (HT_info*)malloc(sizeof(HT_info));
	memcpy(ht_info,p_block_ptr,sizeof(HT_info));
	ht_info->attrName = (char*)malloc(sizeof(char)*ht_info->attrLength);
	memcpy(ht_info->attrName,p_block_ptr+sizeof(HT_info),sizeof(char)*ht_info->attrLength);

	//sychronazition
	//for each bucket in primary index file
	for (i=0; i < MAX_BUCKETS; i++) {
		block_id = ht_info->buckets[i];
		//traverse all blocks in each buckets
		while(1) {
			if (BF_ReadBlock(ht_info->fileDesc,block_id, &p_block_ptr) < 0) {
				BF_PrintError("Error getting block1");return -1;}
			p_block = *(Block*)p_block_ptr;
			//for each record
			for(j=0; j<MAX_RECORDS; j++) {
				if (p_block.bit_array[j] == 1) {
					found = 0;
					//search in secondary index file
					//##############################################################
					//if this record already exists
					//for each bucket in secondary index file
					for (k=0; k < MAX_BUCKETS; k++) {
						block_id2 = sht_info.buckets[k];
						//traverse all blocks in each buckets
						while(1) {
							if (BF_ReadBlock(fileDesc,block_id2, &s_block_ptr) < 0) {
								BF_PrintError("Error getting block2");return -1;}
							s_block = (SecondaryBlock*)s_block_ptr;
							//block_counter++;
							//for each record
							for(m=0; m<MAX_RECORDS; m++) {
								if (s_block->bit_array[m] == 1) {
									if (strcmp(s_block->records[m].name,p_block.records[j].name) == 0
										&& (s_block->records[m].id == p_block.records[j].id) ) {
										found = 1;
									//if it was found,stop searching
										break;
									}
								}
							}
							//if it was found,stop searching
							if (found == 1) {
								break;
							}
							if (s_block->next_block != -1) {
								block_id2 = s_block->next_block;}
							else {
								break;} 
						}
						//if it was found,stop searching
						if (found == 1) {
							break;
						}
					}
					//#######################################################################
					//if this record wasn't found in the secondary index file,insert it
					if (found == 0) {
						SecondaryRecord sec_rec;
					
						sec_rec.blockId = block_id;

						sec_rec.record.id = p_block.records[j].id;
						strcpy(sec_rec.record.name, p_block.records[j].name);
						strcpy(sec_rec.record.surname, p_block.records[j].surname);
						strcpy(sec_rec.record.address, p_block.records[j].address);
						SHT_SecondaryInsertEntry(sht_info, sec_rec);
					}
					
				}
			}
			//go to the next block
			if (p_block.next_block != -1) {
				block_id = p_block.next_block;}
			else {
				break;} 
		}
	}

	if (BF_CloseFile(fileDesc) < 0) {BF_PrintError("Error closing file");return -1;}
	free(sht_info.attrName);
	free(sht_info.fileName);
	return 0;
}
