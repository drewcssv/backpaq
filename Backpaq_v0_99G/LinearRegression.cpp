/* *******************************************************************************
 *  Copyright (C) 2014-2020 Mehmet Gunce Akkoyun Can not be copied and/or
 *	distributed without the express permission of Mehmet Gunce Akkoyun.
 *
 *	Library				: Linear Regression Library
 *	Code Developer		: Mehmet Gunce Akkoyun (akkoyun@me.com)
 *	Revision			: 01.00.01
 *
 *********************************************************************************/

#include "Arduino.h"
#include "LinearRegression.h"

void LinearRegression::Data(float y){

	// Set Data Count
	n++;
	
	// Set X
	float x = n;
	
	// Calclate Variables
    meanX = meanX + ((x-meanX)/n);
    meanX2 = meanX2 + (((x*x)-meanX2)/n);
    varX = meanX2 - (meanX*meanX);
    meanY = meanY + ((y-meanY)/n);
    meanY2 = meanY2 + (((y*y)-meanY2)/n);
    varY = meanY2 - (meanY*meanY);
    meanXY = meanXY + (((x*y)-meanXY)/n);
    covarXY = meanXY - (meanX*meanY);

	// Calculate Coefficients
    m = covarXY / varX;
    b = meanY-(m*meanX);
}
float LinearRegression::Correlation(void) {
    
	// Define Variables
	float stdX = sqrt(varX);
	float stdY = sqrt(varY);
	float stdXstdY = stdX*stdY;
	float correlation;

	// Calculate Correlation
    if(stdXstdY == 0){
		
        correlation = 1;
		
    } else {
		
        correlation = covarXY / stdXstdY;
		
    }
	
	// End Function
    return correlation;
}
float LinearRegression::Calculate(float x) {
    
	// Calculate Y
	return (m*x) + b;
	
}
void LinearRegression::Parameters(float values[]){
    
	values[0] = m;
    values[1] = b;
	
}
uint8_t LinearRegression::Samples(){
    
	return n;
	
}
float LinearRegression::Error(float x, float y) {
    
	return abs(y - Calculate(x));
	
}
void LinearRegression::Reset(){
    
	meanX = 0;
    meanX2 = 0;
    varX = 0;
    meanY = 0;
    meanY2 = 0;
    varY = 0;
    meanXY = 0;
    covarXY = 0;
    n = 0;
    m = 0;
    b = 0;
	
}
