#define NAME_SIZE 15
#define SURNAME_SIZE 20
#define ADDRESS_SIZE 40
#define BLOCK_SIZE 512
#define MAX_BUCKETS 16
#define MAX_RECORDS 6

typedef struct {
	int id;
	char name[NAME_SIZE];
	char surname[SURNAME_SIZE];
	char address[ADDRESS_SIZE];
}Record;

typedef struct{
	Record record;
	int blockId;
}SecondaryRecord;

typedef struct{
	int id;
	char name[NAME_SIZE];
	int blockId;
}my_SecondaryRecord;


typedef struct {
	char recordsCounter;
	Record records[MAX_RECORDS];
	char bit_array[MAX_RECORDS];
	int next_block;
}Block;

typedef struct {
	char recordsCounter;
	my_SecondaryRecord records[MAX_RECORDS];
	char bit_array[MAX_RECORDS]; //1 = record exists,0 = doesnt exist
	int next_block;
}SecondaryBlock;

typedef struct {
	int fileDesc;
	char* attrName;
	char attrType;
	int attrLength;
	long int numBuckets;
	int buckets[MAX_BUCKETS];
}HT_info;

typedef struct {
	int fileDesc;
	char* attrName;
	int attrLength;
	long int numBuckets;
	char *fileName;
	int filesize;
	int buckets[MAX_BUCKETS];
}SHT_info;



int HT_CreateIndex(char *fileName,char attrType,char*attrName,int attrLength,int buckets);

HT_info* HT_OpenIndex(char *fileName);

int HT_CloseIndex(HT_info* header_info);

int HT_InsertEntry(HT_info header,Record record);

int HT_DeleteEntry(HT_info header,void *value);

int HT_GetAllEntries(HT_info header_info,void *value);

int HashStatistics(char* filename);

int hash_int(int ;);

int hash_string(char*);

void HT_PrintEntries(HT_info*);

int SHT_SecondaryInsertEntry( SHT_info header_info, SecondaryRecord record);

int SHT_CreateSecondaryIndex(char *sfileName, char *attrName, int attrLength, int buckets, char *fileName);

SHT_info* SHT_OpenSecondaryIndex( char *sfileName );

int SHT_CloseSecondaryIndex( SHT_info *header_info );

int SHT_SecondaryGetAllEntries(SHT_info header_info_sht, HT_info header_info_ht, void *value);