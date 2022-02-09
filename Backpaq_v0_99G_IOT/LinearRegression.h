/* *******************************************************************************
 *  Copyright (C) 2014-2020 Mehmet Gunce Akkoyun Can not be copied and/or
 *	distributed without the express permission of Mehmet Gunce Akkoyun.
 *
 *	Library				: Linear Regression Library
 *	Code Developer		: Mehmet Gunce Akkoyun (akkoyun@me.com)
 *	Revision			: 01.00.01
 *
 *********************************************************************************/

#ifndef __LinearRegression__
#define __LinearRegression__

#include <Arduino.h>

class LinearRegression {

public:
	
	// ************************************************************
	// Public Functions
	// ************************************************************

	void Data(float y);
	float Calculate(float x);
	float Correlation(void);
	void Parameters(float values[]);
	void Reset(void);
	uint8_t Samples(void);
	float Error(float x, float y);

private:

	// ************************************************************
	// Private Variables
	// ************************************************************

	float meanX = 0;	// Mean X
	float meanX2 = 0;	// Mean X²
	float varX = 0;
	float meanY = 0;	// Mean Y
	float meanY2 = 0;	// Mean Y²
	float varY = 0;
	float meanXY = 0; 	// Mean X*Y
	float covarXY = 0;
	uint8_t n = 0;		// Number of Data
	
	// Y = mX + b
	float m = 0;
	float b = 0;

};

#endif /* defined(__LinearRegression__) */
