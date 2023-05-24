#include <iostream>
#include <fstream>
#include <string>
#include <vector>

unsigned long long packDataHeader(unsigned long long t, unsigned long long p, unsigned long long x,
									unsigned long long three, unsigned long long chip_nr, 
									unsigned long long mode, unsigned long long num_bytes){
	unsigned long long header;
	header = ((num_bytes & 0x000000000000FFFF) << 48) | 
	         ((mode & 0x00000000000000FF) << 40) | 
	         ((chip_nr & 0x00000000000000FF) << 32) | 
	 	     ((three & 0x00000000000000FF) << 24) | 
			 ((x & 0x00000000000000FF) << 16) | 
		     ((p & 0x00000000000000FF) << 8) | 
		     (t & 0x00000000000000FF);

	// check 
    // unsigned short t1 = (unsigned short) header & 0xFF;
	// unsigned short p1 = (unsigned short) (header >> 8) & 0xFF;
	// unsigned short x1 = (unsigned short) (header >> 16) & 0xFF;
	// unsigned short three1 = (unsigned short) (header >> 24) & 0xFF;
	// unsigned short chip_nr1 = (unsigned short) (header >> 32) & 0xFF;
	// unsigned short mode1 = (unsigned short) (header >> 40) & 0xFF;
	// unsigned short num_bytes1 = (unsigned short) (header >> 48) & 0xFFFF;

	// std::cout << "T: " << t1 
	// 	      << ", P: " << p1 
	// 	      << ", X: " << x1 
	// 	      << ", three: " << three1 
	// 	      << ", chip_nr: " << chip_nr1 
	// 		  << ", mode: " << mode1 
	// 		  << ", num_bytes: " << num_bytes1; 

	return header;
}

unsigned long long packPixelHit(unsigned long long spider_time, unsigned long long ftoa, 
				unsigned long long TOT, unsigned long long TOA, unsigned long long pixaddr){
	unsigned long long temp;
	unsigned long long header = 0xb;

	temp = ((header & 0x00000000000000FF) << 60) | 
		   ((pixaddr & 0x000000000000FFFF) << 44) | 
			((TOA & 0x0000000000003FFF) << 30) | 
			((TOT & 0x00000000000003FF) << 20) | 
			((ftoa & 0x00000000000000FF) << 16) | 
			(spider_time & 0x000000000000FFFF);

	// check 
	// unsigned short spider_time1 = (unsigned short) temp & 0xFFFF;
	// unsigned char ftoa1 = (unsigned char) (temp >> 16) & 0xFF;
	// unsigned short TOT1 = (unsigned short) (temp >> 20) & 0x300;
	// unsigned short TOA1 = (unsigned short) (temp >> 30) & 0x3FFF;
	// unsigned short pixaddr1 = (unsigned short) (temp >> 44) & 0xFFFF;
	// unsigned char header1 = (unsigned char) (temp >> 60) & 0xFF;

	// std::cout << "spider_time: " << std::hex << spider_time1 
	// 		  << ", ftoa: " << std::hex  << +ftoa1
	// 		  << ", TOT: " << std::hex << TOT1 
	// 		  << ", TOA: " << std::hex << TOA1  
	// 		  << ", pixaddr: " << std::hex << pixaddr1 
	// 		  << ", header: " << std::hex << +header1 << std::endl;

	unsigned long long spidertime = (spider_time << 14) | TOA;
	std::cout << "spidertime + toa: " << spidertime << " s\n";
	
	return temp;
}

int main(int argc, char** argv){

	// create a .tpx3 file 
	std::ofstream write_file("rollover_test_data.tpx3", std::ios::out | std::ios::binary);
	if (!write_file){
		std::cout << "Cannot open file!" << std::endl;
		return 1;
	}

	unsigned long long temp;
	temp = packDataHeader('T','P','X','3',0,0,208);
	write_file.write((char*) &temp, sizeof(unsigned long long));


	// disorder pixel hit datapackets 
	// std::cout << "Disorder: increment counter" << std::endl;
	// increment counter
	temp = packPixelHit(0xF000,0xFF,0x3FF,0x3FFF,0x9876);   // 25.1662, 1006649343
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0xFF00,0xFF,0x3FF,0x3FFF,0x9876);   // 26.7391, 1069563903
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0xFFFF,0xFF,0x3FF,0x3FFF,0x9876);	// 26.8435, 1073741823
	write_file.write((char*) &temp, sizeof(unsigned long long));

	/* --------------------------------------------------------------*/

	temp = packPixelHit(0x000F,0xFF,0x3FF,0x3FFF,0x9876);   // 0.00655357, 262143
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x00FF,0xFF,0x3FF,0x3FFF,0x9876);   // 0.104858, 4194303
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x0FFF,0xFF,0x3FF,0x3FFF,0x9876);   // 1.67772, 67108863
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x2FFF,0xFF,0x3FF,0x3FFF,0x9876);   // 5.03316, 201326591
	write_file.write((char*) &temp, sizeof(unsigned long long));


	// disorder pixel hit datapackets
	// std::cout << "Disorder: no changes counter" << std::endl;
	// no changes to counter 
	// temp = packDataHeader('T','P','X','3',0,0,48);
	// write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x000F,0xFF,0x3FF,0x3FFF,0x9876);   // 0.00655357, 262143
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x3FFF,0xFF,0x3FF,0x3FFF,0x9876);   // 6.71089, 268435455
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x00FF,0xFF,0x3FF,0x3FFF,0x9876);   // 0.104858, 4194303
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x0FFF,0xFF,0x3FF,0x3FFF,0x9876);   // 1.67772, 67108863
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x1FFF,0xFF,0x3FF,0x3FFF,0x9876);   // 3.35544, 134217727
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x2FFF,0xFF,0x3FF,0x3FFF,0x9876);   // 5.03316, 201326591
	write_file.write((char*) &temp, sizeof(unsigned long long));


	// in order pixel hit datapackets 
	// std::cout << "In order: decrement counter" << std::endl;
	// decrement counter
	// temp = packDataHeader('T','P','X','3',0,0,56);
	// write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x000F,0xFF,0x3FF,0x3FFF,0x9876);   // 0.00655357, 262143
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x00FF,0xFF,0x3FF,0x3FFF,0x9876);   // 0.104858, 4194303
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x0FFF,0xFF,0x3FF,0x3FFF,0x9876);   // 1.67772, 67108863
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x2FFF,0xFF,0x3FF,0x3FFF,0x9876);   // 5.03316, 201326591
	write_file.write((char*) &temp, sizeof(unsigned long long));

	/* --------------------------------------------------------------*/

	temp = packPixelHit(0xF000,0xFF,0x3FF,0x3FFF,0x9876);   // 25.1662, 1006649343
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0xFF00,0xFF,0x3FF,0x3FFF,0x9876);   // 26.7391, 1069563903
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0xFFFF,0xFF,0x3FF,0x3FFF,0x9876);	// 26.8435, 1073741823
	write_file.write((char*) &temp, sizeof(unsigned long long));

	

	// in order pixel 
	// std::cout << "In order: no changes counter" << std::endl;
	// no changes to counter 
	// temp = packDataHeader('T','P','X','3',0,0,48);
	// write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x000F,0xFF,0x3FF,0x3FFF,0x9876);   // 0.00655357, 262143
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x00FF,0xFF,0x3FF,0x3FFF,0x9876);   // 0.104858, 4194303
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x0FFF,0xFF,0x3FF,0x3FFF,0x9876);   // 1.67772, 67108863
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x1FFF,0xFF,0x3FF,0x3FFF,0x9876);   // 3.35544, 134217727
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x2FFF,0xFF,0x3FF,0x3FFF,0x9876);   // 5.03316, 201326591
	write_file.write((char*) &temp, sizeof(unsigned long long));

	temp = packPixelHit(0x3FFF,0xFF,0x3FF,0x3FFF,0x9876);   // 6.71089, 268435455
	write_file.write((char*) &temp, sizeof(unsigned long long));


	write_file.close();

	// read in .tpx3 file 
	std::ifstream read_file("rollover_test_data.tpx3", std::ios::out | std::ios::binary);
	if (!read_file){
		std::cout << "Cannot open file!" << std::endl;
		return 1;
	}

	std::cout << " \n Reading file: Checking \n";

	unsigned long long temp1;
	read_file.read((char*) &temp1, sizeof(unsigned long long));
	unsigned short t1 = (unsigned short) temp1 & 0xFF;
	unsigned short p1 = (unsigned short) (temp1 >> 8) & 0xFF;
	unsigned short x1 = (unsigned short) (temp1 >> 16) & 0xFF;
	unsigned short three1 = (unsigned short) (temp1 >> 24) & 0xFF;
	unsigned short chip_nr1 = (unsigned short) (temp1 >> 32) & 0xFF;
	unsigned short mode1 = (unsigned short) (temp1 >> 40) & 0xFF;
	unsigned short num_bytes1 = (unsigned short) (temp1 >> 48) & 0xFFFF;

	// std::cout << "T: " << t1 
	// 	      << ", P: " << p1 
	// 	      << ", X: " << x1 
	// 	      << ", three: " << three1 
	// 	      << ", chip_nr: " << chip_nr1 
	// 		  << ", mode: " << mode1 
	// 		  << ", num_bytes: " << num_bytes1
	// 		  << std::endl; 

	while (read_file.read((char*) &temp1, sizeof(unsigned long long))){
		// check 
		unsigned short spider_time1 = (unsigned short) temp1 & 0xFFFF;
		unsigned char ftoa1 = (unsigned char) (temp1 >> 16) & 0xFF;
		unsigned short TOT1 = (unsigned short) (temp1 >> 20) & 0x300;
		unsigned short TOA1 = (unsigned short) (temp1 >> 30) & 0x3FFF;
		unsigned short pixaddr1 = (unsigned short) (temp1 >> 44) & 0xFFFF;
		unsigned char header1 = (unsigned char) (temp1 >> 60) & 0xFF;

		// std::cout << "spider_time: " << std::hex << spider_time1 
		// 		  << ", ftoa: " << std::hex  << +ftoa1
		// 		  << ", TOT: " << std::hex << TOT1 
		// 		  << ", TOA: " << std::hex << TOA1  
		// 		  << ", pixaddr: " << std::hex << pixaddr1 
		// 		  << ", header: " << std::hex << +header1 << std::endl;

		unsigned int spidertime1 = (spider_time1 << 14) | TOA1;
		std::cout << "spidertime + toa: " << spidertime1 << " s\n";
	}	
	

	read_file.close();

	return 0;

}