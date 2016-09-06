#ifndef _COMM_STRUCT_H_
#define _COMM_STRUCT_H_

#define PKG_BODY_LEN 1024 * 1024
#pragma pack(1)
typedef struct _PkgHead {
	uint8_t cVer;
	uint16_t wCmd;
	uint32_t dwLength;
} PkgHead;

typedef struct _Pkg {
	char cStx;
	PkgHead stHead;
	char sBody[PKG_BODY_LEN];
	char cEtx;
}Pkg;

#pragma pack()

#ifdef __cplusplus
extern "C" {
#endif 

#ifdef __cplusplus
}
#endif 


#endif
