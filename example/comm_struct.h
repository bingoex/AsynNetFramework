#ifndef _COMM_STRUCT_H_
#define _COMM_STRUCT_H_

#pragma pack(1)
typedef struct _PkgHead {
	uint8_t cVer;
	uint16_t wCmd;
	uint32_t dwLength;
} PkgHead;
#pragma pack()

#ifdef __cplusplus
extern "C" {
#endif 

#ifdef __cplusplus
}
#endif 


#endif
