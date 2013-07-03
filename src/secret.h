// secret.h
// constants to calculate the TOTP
// *msd 7/3/13 

const unsigned char sha1_key[] = {
};

#define SECRET_SIZE 10	// size of the above key in bytes
//#define TIME_ZONE_OFFSET -5  *msd 7/3/13 Not needed any more - httpebble can find this dynamically	
#define DIGITS_TRUNCATE 1000000 // Truncate n decimal digits to 2^n for 6 digits
#define SHA1_SIZE 20
	
#define CDT -5
#define CST -6