/**
 * @file hds_dtypes.h
 * @brief Define custom data types that will be used universally.
 */
#ifndef HDS_DTYPES_H_
#define HDS_DTYPES_H_
/**
 * @def HDS_OK
 * @brief Symbol meaning that a operation has completed sucessfully.
 */
#define HDS_OK 0
/**
 * @enum ON_OFF
 * @brief Creates two symbols on and off having values 1 and 0 respectively.
 */
typedef enum{
	off, /**< Signifies that value is reset */
	on /**< Signifies that value is set */
} ON_OFF;

#endif /* HDS_DTYPES_H_ */
