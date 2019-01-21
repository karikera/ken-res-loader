#include "huffman.h"

#include "tables.h"




//
//internal

namespace OpenMP3
{

	struct HuffmanTable
	{
		const UInt16 * data;
		UInt treelen;
		UInt linbits;
	};

	void HuffmanDecode(Reservoir & br, UInt table_num, int *x, int *y, int *v, int *w);

	extern const UInt16 kHuffmanTableSet[2804];

	extern const HuffmanTable kHuffmanTables[34];

}




//
//impl

OpenMP3::UInt OpenMP3::ReadHuffman(Reservoir & br, UInt sfreq, FrameData::Granule & granule, size_t part_2_start)
{
	auto & is = granule.is;

	//Check that there is any data to decode. If not,zero the array
	if (granule.part2_3_length == 0) 
	{
		for (UInt i = 0; i < 576; i++) is[i] = 0.0f;

		//MemClear(is, 2304);

		return 0;
	}

	int x, y, v, w;

	UInt table_num, is_pos, region_1_start, region_2_start;

	size_t bit_pos_end = part_2_start + granule.part2_3_length - 1; //calc the index of the last bit for this part

	/* Determine region boundaries */
	if (granule.window_switching && (granule.block_type == 2))
	{
		region_1_start = 36;  /* sfb[9/3]*3=36 */

		region_2_start = 576; /* No Region2 for short block case. */
	}
	else 
	{
		region_1_start = kScaleFactorBandIndices[sfreq].l[granule.region0_count + 1];
		
		region_2_start = kScaleFactorBandIndices[sfreq].l[granule.region0_count + granule.region1_count + 2];
	}

	/* Read big_values using tables according to region_x_start */

	auto table_select = granule.table_select;

	for (is_pos = 0; is_pos < granule.big_values * 2; /*is_pos++*/)
	{
		if (is_pos < region_1_start) 
		{
			table_num = table_select[0];
		}
		else if (is_pos < region_2_start) 
		{
			table_num = table_select[1];
		}
		else
		{
			table_num = table_select[2];
		}

		HuffmanDecode(br, table_num, &x, &y, &v, &w);

		is[is_pos++] = Float32(x);

		is[is_pos++] = Float32(y);	//was no ++
	}
	
	/* Read small values until is_pos = 576 or we run out of huffman data */
	table_num = granule.count1table_select + 32;

	for (is_pos = granule.big_values * 2; (is_pos <= 572) && (br.GetPosition() <= bit_pos_end); /*is_pos++*/)
	{
		HuffmanDecode(br, table_num, &x, &y, &v, &w);

		is[is_pos++] = Float32(v);
		
		is[is_pos++] = Float32(w);
		
		is[is_pos++] = Float32(x);
		
		is[is_pos++] = Float32(y);	//was no ++
	}

	if (br.GetPosition() > (bit_pos_end + 1))
	{
		is_pos -= 4; //If read past the end of this section, remove last words read
	}
		
	//g_frame_data.count1[gr][ch] = is_pos;		//Setup count1 which is the index of the first sample in the rzero reg

	for (UInt i = is_pos; i < 576; i++) is[i] = 0.0;	//zero remainder
	
	br.SetPosition(bit_pos_end + 1);	//Set the bitpos to point to the next part to read

	return is_pos; //return count1, index of the first sample in the rzero reg
}

//reads/decodes next Huffman code word from main_data reservoir
void OpenMP3::HuffmanDecode(Reservoir & br, UInt table_num, int *x, int *y, int *v, int *w)
{
	const HuffmanTable & huffmantable = kHuffmanTables[table_num];

	const UInt16 * htptr = huffmantable.data;

	UInt point = 0, error = 1, bitsleft = 32; //=16??

	UInt treelen = huffmantable.treelen;

	UInt linbits = huffmantable.linbits;

	if (treelen == 0)
	{
		*x = *y = *v = *w = 0;
	}
	else
	{
		do
		{   
			//start reading the Huffman code word, bit by bit

			if ((htptr[point] & 0xff00) == 0) //find a code word
			{
				error = 0;

				*x = (htptr[point] >> 4) & 0xf;

				*y = htptr[point] & 0xf;

				break;
			}

			if (br.ReadBit())
			{
				while ((htptr[point] & 0xff) >= 250) point += htptr[point] & 0xff; //go right in tree

				point += htptr[point] & 0xff;
			}
			else
			{
				while ((htptr[point] >> 8) >= 250) point += htptr[point] >> 8; //go left in tree

				point += htptr[point] >> 8;
			}
		} 
		while ((--bitsleft > 0) && (point < treelen));

		if (table_num > 31)
		{
			//Process sign encodings for quadruples tables

			*v = (*y >> 3) & 1;
			*w = (*y >> 2) & 1;
			*x = (*y >> 1) & 1;
			*y = *y & 1;

			if ((*v > 0) && (br.ReadBit() == 1)) *v = -*v;
			if ((*w > 0) && (br.ReadBit() == 1)) *w = -*w;
			if ((*x > 0) && (br.ReadBit() == 1)) *x = -*x;
			if ((*y > 0) && (br.ReadBit() == 1)) *y = -*y;
		}
		else
		{
			if ((linbits > 0) && (*x == 15)) *x += br.ReadBits(linbits); //Get linbits

			if ((*x > 0) && (br.ReadBit() == 1)) *x = -*x; //Get sign bit

			if ((linbits > 0) && (*y == 15)) *y += br.ReadBits(linbits); //Get linbits

			if ((*y > 0) && (br.ReadBit() == 1)) *y = -*y; //Get sign bit
		}
	}
}




//
//data

const OpenMP3::UInt16 OpenMP3::kHuffmanTableSet[] =
{
	//g_huffman_table_1[7] = {
	0x0201,0x0000,0x0201,0x0010,0x0201,0x0001,0x0011,
	//},g_huffman_table_2[17] = {
	0x0201,0x0000,0x0401,0x0201,0x0010,0x0001,0x0201,0x0011,0x0401,0x0201,0x0020,
	0x0021,0x0201,0x0012,0x0201,0x0002,0x0022,
	//},g_huffman_table_3[17] = {
	0x0401,0x0201,0x0000,0x0001,0x0201,0x0011,0x0201,0x0010,0x0401,0x0201,0x0020,
	0x0021,0x0201,0x0012,0x0201,0x0002,0x0022,
	//},g_huffman_table_5[31] = {
	0x0201,0x0000,0x0401,0x0201,0x0010,0x0001,0x0201,0x0011,0x0801,0x0401,0x0201,
	0x0020,0x0002,0x0201,0x0021,0x0012,0x0801,0x0401,0x0201,0x0022,0x0030,0x0201,
	0x0003,0x0013,0x0201,0x0031,0x0201,0x0032,0x0201,0x0023,0x0033,
	//},g_huffman_table_6[31] = {
	0x0601,0x0401,0x0201,0x0000,0x0010,0x0011,0x0601,0x0201,0x0001,0x0201,0x0020,
	0x0021,0x0601,0x0201,0x0012,0x0201,0x0002,0x0022,0x0401,0x0201,0x0031,0x0013,
	0x0401,0x0201,0x0030,0x0032,0x0201,0x0023,0x0201,0x0003,0x0033,
	//},g_huffman_table_7[71] = {
	0x0201,0x0000,0x0401,0x0201,0x0010,0x0001,0x0801,0x0201,0x0011,0x0401,0x0201,
	0x0020,0x0002,0x0021,0x1201,0x0601,0x0201,0x0012,0x0201,0x0022,0x0030,0x0401,
	0x0201,0x0031,0x0013,0x0401,0x0201,0x0003,0x0032,0x0201,0x0023,0x0004,0x0a01,
	0x0401,0x0201,0x0040,0x0041,0x0201,0x0014,0x0201,0x0042,0x0024,0x0c01,0x0601,
	0x0401,0x0201,0x0033,0x0043,0x0050,0x0401,0x0201,0x0034,0x0005,0x0051,0x0601,
	0x0201,0x0015,0x0201,0x0052,0x0025,0x0401,0x0201,0x0044,0x0035,0x0401,0x0201,
	0x0053,0x0054,0x0201,0x0045,0x0055,
	//},g_huffman_table_8[71] = {
	0x0601,0x0201,0x0000,0x0201,0x0010,0x0001,0x0201,0x0011,0x0401,0x0201,0x0021,
	0x0012,0x0e01,0x0401,0x0201,0x0020,0x0002,0x0201,0x0022,0x0401,0x0201,0x0030,
	0x0003,0x0201,0x0031,0x0013,0x0e01,0x0801,0x0401,0x0201,0x0032,0x0023,0x0201,
	0x0040,0x0004,0x0201,0x0041,0x0201,0x0014,0x0042,0x0c01,0x0601,0x0201,0x0024,
	0x0201,0x0033,0x0050,0x0401,0x0201,0x0043,0x0034,0x0051,0x0601,0x0201,0x0015,
	0x0201,0x0005,0x0052,0x0601,0x0201,0x0025,0x0201,0x0044,0x0035,0x0201,0x0053,
	0x0201,0x0045,0x0201,0x0054,0x0055,
	//},g_huffman_table_9[71] = {
	0x0801,0x0401,0x0201,0x0000,0x0010,0x0201,0x0001,0x0011,0x0a01,0x0401,0x0201,
	0x0020,0x0021,0x0201,0x0012,0x0201,0x0002,0x0022,0x0c01,0x0601,0x0401,0x0201,
	0x0030,0x0003,0x0031,0x0201,0x0013,0x0201,0x0032,0x0023,0x0c01,0x0401,0x0201,
	0x0041,0x0014,0x0401,0x0201,0x0040,0x0033,0x0201,0x0042,0x0024,0x0a01,0x0601,
	0x0401,0x0201,0x0004,0x0050,0x0043,0x0201,0x0034,0x0051,0x0801,0x0401,0x0201,
	0x0015,0x0052,0x0201,0x0025,0x0044,0x0601,0x0401,0x0201,0x0005,0x0054,0x0053,
	0x0201,0x0035,0x0201,0x0045,0x0055,
	//},g_huffman_table_10[127] = {
	0x0201,0x0000,0x0401,0x0201,0x0010,0x0001,0x0a01,0x0201,0x0011,0x0401,0x0201,
	0x0020,0x0002,0x0201,0x0021,0x0012,0x1c01,0x0801,0x0401,0x0201,0x0022,0x0030,
	0x0201,0x0031,0x0013,0x0801,0x0401,0x0201,0x0003,0x0032,0x0201,0x0023,0x0040,
	0x0401,0x0201,0x0041,0x0014,0x0401,0x0201,0x0004,0x0033,0x0201,0x0042,0x0024,
	0x1c01,0x0a01,0x0601,0x0401,0x0201,0x0050,0x0005,0x0060,0x0201,0x0061,0x0016,
	0x0c01,0x0601,0x0401,0x0201,0x0043,0x0034,0x0051,0x0201,0x0015,0x0201,0x0052,
	0x0025,0x0401,0x0201,0x0026,0x0036,0x0071,0x1401,0x0801,0x0201,0x0017,0x0401,
	0x0201,0x0044,0x0053,0x0006,0x0601,0x0401,0x0201,0x0035,0x0045,0x0062,0x0201,
	0x0070,0x0201,0x0007,0x0064,0x0e01,0x0401,0x0201,0x0072,0x0027,0x0601,0x0201,
	0x0063,0x0201,0x0054,0x0055,0x0201,0x0046,0x0073,0x0801,0x0401,0x0201,0x0037,
	0x0065,0x0201,0x0056,0x0074,0x0601,0x0201,0x0047,0x0201,0x0066,0x0075,0x0401,
	0x0201,0x0057,0x0076,0x0201,0x0067,0x0077,
	//},g_huffman_table_11[127] = {
	0x0601,0x0201,0x0000,0x0201,0x0010,0x0001,0x0801,0x0201,0x0011,0x0401,0x0201,
	0x0020,0x0002,0x0012,0x1801,0x0801,0x0201,0x0021,0x0201,0x0022,0x0201,0x0030,
	0x0003,0x0401,0x0201,0x0031,0x0013,0x0401,0x0201,0x0032,0x0023,0x0401,0x0201,
	0x0040,0x0004,0x0201,0x0041,0x0014,0x1e01,0x1001,0x0a01,0x0401,0x0201,0x0042,
	0x0024,0x0401,0x0201,0x0033,0x0043,0x0050,0x0401,0x0201,0x0034,0x0051,0x0061,
	0x0601,0x0201,0x0016,0x0201,0x0006,0x0026,0x0201,0x0062,0x0201,0x0015,0x0201,
	0x0005,0x0052,0x1001,0x0a01,0x0601,0x0401,0x0201,0x0025,0x0044,0x0060,0x0201,
	0x0063,0x0036,0x0401,0x0201,0x0070,0x0017,0x0071,0x1001,0x0601,0x0401,0x0201,
	0x0007,0x0064,0x0072,0x0201,0x0027,0x0401,0x0201,0x0053,0x0035,0x0201,0x0054,
	0x0045,0x0a01,0x0401,0x0201,0x0046,0x0073,0x0201,0x0037,0x0201,0x0065,0x0056,
	0x0a01,0x0601,0x0401,0x0201,0x0055,0x0057,0x0074,0x0201,0x0047,0x0066,0x0401,
	0x0201,0x0075,0x0076,0x0201,0x0067,0x0077,
	//},g_huffman_table_12[127] = {
	0x0c01,0x0401,0x0201,0x0010,0x0001,0x0201,0x0011,0x0201,0x0000,0x0201,0x0020,
	0x0002,0x1001,0x0401,0x0201,0x0021,0x0012,0x0401,0x0201,0x0022,0x0031,0x0201,
	0x0013,0x0201,0x0030,0x0201,0x0003,0x0040,0x1a01,0x0801,0x0401,0x0201,0x0032,
	0x0023,0x0201,0x0041,0x0033,0x0a01,0x0401,0x0201,0x0014,0x0042,0x0201,0x0024,
	0x0201,0x0004,0x0050,0x0401,0x0201,0x0043,0x0034,0x0201,0x0051,0x0015,0x1c01,
	0x0e01,0x0801,0x0401,0x0201,0x0052,0x0025,0x0201,0x0053,0x0035,0x0401,0x0201,
	0x0060,0x0016,0x0061,0x0401,0x0201,0x0062,0x0026,0x0601,0x0401,0x0201,0x0005,
	0x0006,0x0044,0x0201,0x0054,0x0045,0x1201,0x0a01,0x0401,0x0201,0x0063,0x0036,
	0x0401,0x0201,0x0070,0x0007,0x0071,0x0401,0x0201,0x0017,0x0064,0x0201,0x0046,
	0x0072,0x0a01,0x0601,0x0201,0x0027,0x0201,0x0055,0x0073,0x0201,0x0037,0x0056,
	0x0801,0x0401,0x0201,0x0065,0x0074,0x0201,0x0047,0x0066,0x0401,0x0201,0x0075,
	0x0057,0x0201,0x0076,0x0201,0x0067,0x0077,
	//},g_huffman_table_13[511] = {
	0x0201,0x0000,0x0601,0x0201,0x0010,0x0201,0x0001,0x0011,0x1c01,0x0801,0x0401,
	0x0201,0x0020,0x0002,0x0201,0x0021,0x0012,0x0801,0x0401,0x0201,0x0022,0x0030,
	0x0201,0x0003,0x0031,0x0601,0x0201,0x0013,0x0201,0x0032,0x0023,0x0401,0x0201,
	0x0040,0x0004,0x0041,0x4601,0x1c01,0x0e01,0x0601,0x0201,0x0014,0x0201,0x0033,
	0x0042,0x0401,0x0201,0x0024,0x0050,0x0201,0x0043,0x0034,0x0401,0x0201,0x0051,
	0x0015,0x0401,0x0201,0x0005,0x0052,0x0201,0x0025,0x0201,0x0044,0x0053,0x0e01,
	0x0801,0x0401,0x0201,0x0060,0x0006,0x0201,0x0061,0x0016,0x0401,0x0201,0x0080,
	0x0008,0x0081,0x1001,0x0801,0x0401,0x0201,0x0035,0x0062,0x0201,0x0026,0x0054,
	0x0401,0x0201,0x0045,0x0063,0x0201,0x0036,0x0070,0x0601,0x0401,0x0201,0x0007,
	0x0055,0x0071,0x0201,0x0017,0x0201,0x0027,0x0037,0x4801,0x1801,0x0c01,0x0401,
	0x0201,0x0018,0x0082,0x0201,0x0028,0x0401,0x0201,0x0064,0x0046,0x0072,0x0801,
	0x0401,0x0201,0x0084,0x0048,0x0201,0x0090,0x0009,0x0201,0x0091,0x0019,0x1801,
	0x0e01,0x0801,0x0401,0x0201,0x0073,0x0065,0x0201,0x0056,0x0074,0x0401,0x0201,
	0x0047,0x0066,0x0083,0x0601,0x0201,0x0038,0x0201,0x0075,0x0057,0x0201,0x0092,
	0x0029,0x0e01,0x0801,0x0401,0x0201,0x0067,0x0085,0x0201,0x0058,0x0039,0x0201,
	0x0093,0x0201,0x0049,0x0086,0x0601,0x0201,0x00a0,0x0201,0x0068,0x000a,0x0201,
	0x00a1,0x001a,0x4401,0x1801,0x0c01,0x0401,0x0201,0x00a2,0x002a,0x0401,0x0201,
	0x0095,0x0059,0x0201,0x00a3,0x003a,0x0801,0x0401,0x0201,0x004a,0x0096,0x0201,
	0x00b0,0x000b,0x0201,0x00b1,0x001b,0x1401,0x0801,0x0201,0x00b2,0x0401,0x0201,
	0x0076,0x0077,0x0094,0x0601,0x0401,0x0201,0x0087,0x0078,0x00a4,0x0401,0x0201,
	0x0069,0x00a5,0x002b,0x0c01,0x0601,0x0401,0x0201,0x005a,0x0088,0x00b3,0x0201,
	0x003b,0x0201,0x0079,0x00a6,0x0601,0x0401,0x0201,0x006a,0x00b4,0x00c0,0x0401,
	0x0201,0x000c,0x0098,0x00c1,0x3c01,0x1601,0x0a01,0x0601,0x0201,0x001c,0x0201,
	0x0089,0x00b5,0x0201,0x005b,0x00c2,0x0401,0x0201,0x002c,0x003c,0x0401,0x0201,
	0x00b6,0x006b,0x0201,0x00c4,0x004c,0x1001,0x0801,0x0401,0x0201,0x00a8,0x008a,
	0x0201,0x00d0,0x000d,0x0201,0x00d1,0x0201,0x004b,0x0201,0x0097,0x00a7,0x0c01,
	0x0601,0x0201,0x00c3,0x0201,0x007a,0x0099,0x0401,0x0201,0x00c5,0x005c,0x00b7,
	0x0401,0x0201,0x001d,0x00d2,0x0201,0x002d,0x0201,0x007b,0x00d3,0x3401,0x1c01,
	0x0c01,0x0401,0x0201,0x003d,0x00c6,0x0401,0x0201,0x006c,0x00a9,0x0201,0x009a,
	0x00d4,0x0801,0x0401,0x0201,0x00b8,0x008b,0x0201,0x004d,0x00c7,0x0401,0x0201,
	0x007c,0x00d5,0x0201,0x005d,0x00e0,0x0a01,0x0401,0x0201,0x00e1,0x001e,0x0401,
	0x0201,0x000e,0x002e,0x00e2,0x0801,0x0401,0x0201,0x00e3,0x006d,0x0201,0x008c,
	0x00e4,0x0401,0x0201,0x00e5,0x00ba,0x00f0,0x2601,0x1001,0x0401,0x0201,0x00f1,
	0x001f,0x0601,0x0401,0x0201,0x00aa,0x009b,0x00b9,0x0201,0x003e,0x0201,0x00d6,
	0x00c8,0x0c01,0x0601,0x0201,0x004e,0x0201,0x00d7,0x007d,0x0201,0x00ab,0x0201,
	0x005e,0x00c9,0x0601,0x0201,0x000f,0x0201,0x009c,0x006e,0x0201,0x00f2,0x002f,
	0x2001,0x1001,0x0601,0x0401,0x0201,0x00d8,0x008d,0x003f,0x0601,0x0201,0x00f3,
	0x0201,0x00e6,0x00ca,0x0201,0x00f4,0x004f,0x0801,0x0401,0x0201,0x00bb,0x00ac,
	0x0201,0x00e7,0x00f5,0x0401,0x0201,0x00d9,0x009d,0x0201,0x005f,0x00e8,0x1e01,
	0x0c01,0x0601,0x0201,0x006f,0x0201,0x00f6,0x00cb,0x0401,0x0201,0x00bc,0x00ad,
	0x00da,0x0801,0x0201,0x00f7,0x0401,0x0201,0x007e,0x007f,0x008e,0x0601,0x0401,
	0x0201,0x009e,0x00ae,0x00cc,0x0201,0x00f8,0x008f,0x1201,0x0801,0x0401,0x0201,
	0x00db,0x00bd,0x0201,0x00ea,0x00f9,0x0401,0x0201,0x009f,0x00eb,0x0201,0x00be,
	0x0201,0x00cd,0x00fa,0x0e01,0x0401,0x0201,0x00dd,0x00ec,0x0601,0x0401,0x0201,
	0x00e9,0x00af,0x00dc,0x0201,0x00ce,0x00fb,0x0801,0x0401,0x0201,0x00bf,0x00de,
	0x0201,0x00cf,0x00ee,0x0401,0x0201,0x00df,0x00ef,0x0201,0x00ff,0x0201,0x00ed,
	0x0201,0x00fd,0x0201,0x00fc,0x00fe,
	//},g_huffman_table_15[511] = {
	0x1001,0x0601,0x0201,0x0000,0x0201,0x0010,0x0001,0x0201,0x0011,0x0401,0x0201,
	0x0020,0x0002,0x0201,0x0021,0x0012,0x3201,0x1001,0x0601,0x0201,0x0022,0x0201,
	0x0030,0x0031,0x0601,0x0201,0x0013,0x0201,0x0003,0x0040,0x0201,0x0032,0x0023,
	0x0e01,0x0601,0x0401,0x0201,0x0004,0x0014,0x0041,0x0401,0x0201,0x0033,0x0042,
	0x0201,0x0024,0x0043,0x0a01,0x0601,0x0201,0x0034,0x0201,0x0050,0x0005,0x0201,
	0x0051,0x0015,0x0401,0x0201,0x0052,0x0025,0x0401,0x0201,0x0044,0x0053,0x0061,
	0x5a01,0x2401,0x1201,0x0a01,0x0601,0x0201,0x0035,0x0201,0x0060,0x0006,0x0201,
	0x0016,0x0062,0x0401,0x0201,0x0026,0x0054,0x0201,0x0045,0x0063,0x0a01,0x0601,
	0x0201,0x0036,0x0201,0x0070,0x0007,0x0201,0x0071,0x0055,0x0401,0x0201,0x0017,
	0x0064,0x0201,0x0072,0x0027,0x1801,0x1001,0x0801,0x0401,0x0201,0x0046,0x0073,
	0x0201,0x0037,0x0065,0x0401,0x0201,0x0056,0x0080,0x0201,0x0008,0x0074,0x0401,
	0x0201,0x0081,0x0018,0x0201,0x0082,0x0028,0x1001,0x0801,0x0401,0x0201,0x0047,
	0x0066,0x0201,0x0083,0x0038,0x0401,0x0201,0x0075,0x0057,0x0201,0x0084,0x0048,
	0x0601,0x0401,0x0201,0x0090,0x0019,0x0091,0x0401,0x0201,0x0092,0x0076,0x0201,
	0x0067,0x0029,0x5c01,0x2401,0x1201,0x0a01,0x0401,0x0201,0x0085,0x0058,0x0401,
	0x0201,0x0009,0x0077,0x0093,0x0401,0x0201,0x0039,0x0094,0x0201,0x0049,0x0086,
	0x0a01,0x0601,0x0201,0x0068,0x0201,0x00a0,0x000a,0x0201,0x00a1,0x001a,0x0401,
	0x0201,0x00a2,0x002a,0x0201,0x0095,0x0059,0x1a01,0x0e01,0x0601,0x0201,0x00a3,
	0x0201,0x003a,0x0087,0x0401,0x0201,0x0078,0x00a4,0x0201,0x004a,0x0096,0x0601,
	0x0401,0x0201,0x0069,0x00b0,0x00b1,0x0401,0x0201,0x001b,0x00a5,0x00b2,0x0e01,
	0x0801,0x0401,0x0201,0x005a,0x002b,0x0201,0x0088,0x0097,0x0201,0x00b3,0x0201,
	0x0079,0x003b,0x0801,0x0401,0x0201,0x006a,0x00b4,0x0201,0x004b,0x00c1,0x0401,
	0x0201,0x0098,0x0089,0x0201,0x001c,0x00b5,0x5001,0x2201,0x1001,0x0601,0x0401,
	0x0201,0x005b,0x002c,0x00c2,0x0601,0x0401,0x0201,0x000b,0x00c0,0x00a6,0x0201,
	0x00a7,0x007a,0x0a01,0x0401,0x0201,0x00c3,0x003c,0x0401,0x0201,0x000c,0x0099,
	0x00b6,0x0401,0x0201,0x006b,0x00c4,0x0201,0x004c,0x00a8,0x1401,0x0a01,0x0401,
	0x0201,0x008a,0x00c5,0x0401,0x0201,0x00d0,0x005c,0x00d1,0x0401,0x0201,0x00b7,
	0x007b,0x0201,0x001d,0x0201,0x000d,0x002d,0x0c01,0x0401,0x0201,0x00d2,0x00d3,
	0x0401,0x0201,0x003d,0x00c6,0x0201,0x006c,0x00a9,0x0601,0x0401,0x0201,0x009a,
	0x00b8,0x00d4,0x0401,0x0201,0x008b,0x004d,0x0201,0x00c7,0x007c,0x4401,0x2201,
	0x1201,0x0a01,0x0401,0x0201,0x00d5,0x005d,0x0401,0x0201,0x00e0,0x000e,0x00e1,
	0x0401,0x0201,0x001e,0x00e2,0x0201,0x00aa,0x002e,0x0801,0x0401,0x0201,0x00b9,
	0x009b,0x0201,0x00e3,0x00d6,0x0401,0x0201,0x006d,0x003e,0x0201,0x00c8,0x008c,
	0x1001,0x0801,0x0401,0x0201,0x00e4,0x004e,0x0201,0x00d7,0x007d,0x0401,0x0201,
	0x00e5,0x00ba,0x0201,0x00ab,0x005e,0x0801,0x0401,0x0201,0x00c9,0x009c,0x0201,
	0x00f1,0x001f,0x0601,0x0401,0x0201,0x00f0,0x006e,0x00f2,0x0201,0x002f,0x00e6,
	0x2601,0x1201,0x0801,0x0401,0x0201,0x00d8,0x00f3,0x0201,0x003f,0x00f4,0x0601,
	0x0201,0x004f,0x0201,0x008d,0x00d9,0x0201,0x00bb,0x00ca,0x0801,0x0401,0x0201,
	0x00ac,0x00e7,0x0201,0x007e,0x00f5,0x0801,0x0401,0x0201,0x009d,0x005f,0x0201,
	0x00e8,0x008e,0x0201,0x00f6,0x00cb,0x2201,0x1201,0x0a01,0x0601,0x0401,0x0201,
	0x000f,0x00ae,0x006f,0x0201,0x00bc,0x00da,0x0401,0x0201,0x00ad,0x00f7,0x0201,
	0x007f,0x00e9,0x0801,0x0401,0x0201,0x009e,0x00cc,0x0201,0x00f8,0x008f,0x0401,
	0x0201,0x00db,0x00bd,0x0201,0x00ea,0x00f9,0x1001,0x0801,0x0401,0x0201,0x009f,
	0x00dc,0x0201,0x00cd,0x00eb,0x0401,0x0201,0x00be,0x00fa,0x0201,0x00af,0x00dd,
	0x0e01,0x0601,0x0401,0x0201,0x00ec,0x00ce,0x00fb,0x0401,0x0201,0x00bf,0x00ed,
	0x0201,0x00de,0x00fc,0x0601,0x0401,0x0201,0x00cf,0x00fd,0x00ee,0x0401,0x0201,
	0x00df,0x00fe,0x0201,0x00ef,0x00ff,
	//},g_huffman_table_16[511] = {
	0x0201,0x0000,0x0601,0x0201,0x0010,0x0201,0x0001,0x0011,0x2a01,0x0801,0x0401,
	0x0201,0x0020,0x0002,0x0201,0x0021,0x0012,0x0a01,0x0601,0x0201,0x0022,0x0201,
	0x0030,0x0003,0x0201,0x0031,0x0013,0x0a01,0x0401,0x0201,0x0032,0x0023,0x0401,
	0x0201,0x0040,0x0004,0x0041,0x0601,0x0201,0x0014,0x0201,0x0033,0x0042,0x0401,
	0x0201,0x0024,0x0050,0x0201,0x0043,0x0034,0x8a01,0x2801,0x1001,0x0601,0x0401,
	0x0201,0x0005,0x0015,0x0051,0x0401,0x0201,0x0052,0x0025,0x0401,0x0201,0x0044,
	0x0035,0x0053,0x0a01,0x0601,0x0401,0x0201,0x0060,0x0006,0x0061,0x0201,0x0016,
	0x0062,0x0801,0x0401,0x0201,0x0026,0x0054,0x0201,0x0045,0x0063,0x0401,0x0201,
	0x0036,0x0070,0x0071,0x2801,0x1201,0x0801,0x0201,0x0017,0x0201,0x0007,0x0201,
	0x0055,0x0064,0x0401,0x0201,0x0072,0x0027,0x0401,0x0201,0x0046,0x0065,0x0073,
	0x0a01,0x0601,0x0201,0x0037,0x0201,0x0056,0x0008,0x0201,0x0080,0x0081,0x0601,
	0x0201,0x0018,0x0201,0x0074,0x0047,0x0201,0x0082,0x0201,0x0028,0x0066,0x1801,
	0x0e01,0x0801,0x0401,0x0201,0x0083,0x0038,0x0201,0x0075,0x0084,0x0401,0x0201,
	0x0048,0x0090,0x0091,0x0601,0x0201,0x0019,0x0201,0x0009,0x0076,0x0201,0x0092,
	0x0029,0x0e01,0x0801,0x0401,0x0201,0x0085,0x0058,0x0201,0x0093,0x0039,0x0401,
	0x0201,0x00a0,0x000a,0x001a,0x0801,0x0201,0x00a2,0x0201,0x0067,0x0201,0x0057,
	0x0049,0x0601,0x0201,0x0094,0x0201,0x0077,0x0086,0x0201,0x00a1,0x0201,0x0068,
	0x0095,0xdc01,0x7e01,0x3201,0x1a01,0x0c01,0x0601,0x0201,0x002a,0x0201,0x0059,
	0x003a,0x0201,0x00a3,0x0201,0x0087,0x0078,0x0801,0x0401,0x0201,0x00a4,0x004a,
	0x0201,0x0096,0x0069,0x0401,0x0201,0x00b0,0x000b,0x00b1,0x0a01,0x0401,0x0201,
	0x001b,0x00b2,0x0201,0x002b,0x0201,0x00a5,0x005a,0x0601,0x0201,0x00b3,0x0201,
	0x00a6,0x006a,0x0401,0x0201,0x00b4,0x004b,0x0201,0x000c,0x00c1,0x1e01,0x0e01,
	0x0601,0x0401,0x0201,0x00b5,0x00c2,0x002c,0x0401,0x0201,0x00a7,0x00c3,0x0201,
	0x006b,0x00c4,0x0801,0x0201,0x001d,0x0401,0x0201,0x0088,0x0097,0x003b,0x0401,
	0x0201,0x00d1,0x00d2,0x0201,0x002d,0x00d3,0x1201,0x0601,0x0401,0x0201,0x001e,
	0x002e,0x00e2,0x0601,0x0401,0x0201,0x0079,0x0098,0x00c0,0x0201,0x001c,0x0201,
	0x0089,0x005b,0x0e01,0x0601,0x0201,0x003c,0x0201,0x007a,0x00b6,0x0401,0x0201,
	0x004c,0x0099,0x0201,0x00a8,0x008a,0x0601,0x0201,0x000d,0x0201,0x00c5,0x005c,
	0x0401,0x0201,0x003d,0x00c6,0x0201,0x006c,0x009a,0x5801,0x5601,0x2401,0x1001,
	0x0801,0x0401,0x0201,0x008b,0x004d,0x0201,0x00c7,0x007c,0x0401,0x0201,0x00d5,
	0x005d,0x0201,0x00e0,0x000e,0x0801,0x0201,0x00e3,0x0401,0x0201,0x00d0,0x00b7,
	0x007b,0x0601,0x0401,0x0201,0x00a9,0x00b8,0x00d4,0x0201,0x00e1,0x0201,0x00aa,
	0x00b9,0x1801,0x0a01,0x0601,0x0401,0x0201,0x009b,0x00d6,0x006d,0x0201,0x003e,
	0x00c8,0x0601,0x0401,0x0201,0x008c,0x00e4,0x004e,0x0401,0x0201,0x00d7,0x00e5,
	0x0201,0x00ba,0x00ab,0x0c01,0x0401,0x0201,0x009c,0x00e6,0x0401,0x0201,0x006e,
	0x00d8,0x0201,0x008d,0x00bb,0x0801,0x0401,0x0201,0x00e7,0x009d,0x0201,0x00e8,
	0x008e,0x0401,0x0201,0x00cb,0x00bc,0x009e,0x00f1,0x0201,0x001f,0x0201,0x000f,
	0x002f,0x4201,0x3801,0x0201,0x00f2,0x3401,0x3201,0x1401,0x0801,0x0201,0x00bd,
	0x0201,0x005e,0x0201,0x007d,0x00c9,0x0601,0x0201,0x00ca,0x0201,0x00ac,0x007e,
	0x0401,0x0201,0x00da,0x00ad,0x00cc,0x0a01,0x0601,0x0201,0x00ae,0x0201,0x00db,
	0x00dc,0x0201,0x00cd,0x00be,0x0601,0x0401,0x0201,0x00eb,0x00ed,0x00ee,0x0601,
	0x0401,0x0201,0x00d9,0x00ea,0x00e9,0x0201,0x00de,0x0401,0x0201,0x00dd,0x00ec,
	0x00ce,0x003f,0x00f0,0x0401,0x0201,0x00f3,0x00f4,0x0201,0x004f,0x0201,0x00f5,
	0x005f,0x0a01,0x0201,0x00ff,0x0401,0x0201,0x00f6,0x006f,0x0201,0x00f7,0x007f,
	0x0c01,0x0601,0x0201,0x008f,0x0201,0x00f8,0x00f9,0x0401,0x0201,0x009f,0x00fa,
	0x00af,0x0801,0x0401,0x0201,0x00fb,0x00bf,0x0201,0x00fc,0x00cf,0x0401,0x0201,
	0x00fd,0x00df,0x0201,0x00fe,0x00ef,
	//},g_huffman_table_24[512] = {
	0x3c01,0x0801,0x0401,0x0201,0x0000,0x0010,0x0201,0x0001,0x0011,0x0e01,0x0601,
	0x0401,0x0201,0x0020,0x0002,0x0021,0x0201,0x0012,0x0201,0x0022,0x0201,0x0030,
	0x0003,0x0e01,0x0401,0x0201,0x0031,0x0013,0x0401,0x0201,0x0032,0x0023,0x0401,
	0x0201,0x0040,0x0004,0x0041,0x0801,0x0401,0x0201,0x0014,0x0033,0x0201,0x0042,
	0x0024,0x0601,0x0401,0x0201,0x0043,0x0034,0x0051,0x0601,0x0401,0x0201,0x0050,
	0x0005,0x0015,0x0201,0x0052,0x0025,0xfa01,0x6201,0x2201,0x1201,0x0a01,0x0401,
	0x0201,0x0044,0x0053,0x0201,0x0035,0x0201,0x0060,0x0006,0x0401,0x0201,0x0061,
	0x0016,0x0201,0x0062,0x0026,0x0801,0x0401,0x0201,0x0054,0x0045,0x0201,0x0063,
	0x0036,0x0401,0x0201,0x0071,0x0055,0x0201,0x0064,0x0046,0x2001,0x0e01,0x0601,
	0x0201,0x0072,0x0201,0x0027,0x0037,0x0201,0x0073,0x0401,0x0201,0x0070,0x0007,
	0x0017,0x0a01,0x0401,0x0201,0x0065,0x0056,0x0401,0x0201,0x0080,0x0008,0x0081,
	0x0401,0x0201,0x0074,0x0047,0x0201,0x0018,0x0082,0x1001,0x0801,0x0401,0x0201,
	0x0028,0x0066,0x0201,0x0083,0x0038,0x0401,0x0201,0x0075,0x0057,0x0201,0x0084,
	0x0048,0x0801,0x0401,0x0201,0x0091,0x0019,0x0201,0x0092,0x0076,0x0401,0x0201,
	0x0067,0x0029,0x0201,0x0085,0x0058,0x5c01,0x2201,0x1001,0x0801,0x0401,0x0201,
	0x0093,0x0039,0x0201,0x0094,0x0049,0x0401,0x0201,0x0077,0x0086,0x0201,0x0068,
	0x00a1,0x0801,0x0401,0x0201,0x00a2,0x002a,0x0201,0x0095,0x0059,0x0401,0x0201,
	0x00a3,0x003a,0x0201,0x0087,0x0201,0x0078,0x004a,0x1601,0x0c01,0x0401,0x0201,
	0x00a4,0x0096,0x0401,0x0201,0x0069,0x00b1,0x0201,0x001b,0x00a5,0x0601,0x0201,
	0x00b2,0x0201,0x005a,0x002b,0x0201,0x0088,0x00b3,0x1001,0x0a01,0x0601,0x0201,
	0x0090,0x0201,0x0009,0x00a0,0x0201,0x0097,0x0079,0x0401,0x0201,0x00a6,0x006a,
	0x00b4,0x0c01,0x0601,0x0201,0x001a,0x0201,0x000a,0x00b0,0x0201,0x003b,0x0201,
	0x000b,0x00c0,0x0401,0x0201,0x004b,0x00c1,0x0201,0x0098,0x0089,0x4301,0x2201,
	0x1001,0x0801,0x0401,0x0201,0x001c,0x00b5,0x0201,0x005b,0x00c2,0x0401,0x0201,
	0x002c,0x00a7,0x0201,0x007a,0x00c3,0x0a01,0x0601,0x0201,0x003c,0x0201,0x000c,
	0x00d0,0x0201,0x00b6,0x006b,0x0401,0x0201,0x00c4,0x004c,0x0201,0x0099,0x00a8,
	0x1001,0x0801,0x0401,0x0201,0x008a,0x00c5,0x0201,0x005c,0x00d1,0x0401,0x0201,
	0x00b7,0x007b,0x0201,0x001d,0x00d2,0x0901,0x0401,0x0201,0x002d,0x00d3,0x0201,
	0x003d,0x00c6,0x55fa,0x0401,0x0201,0x006c,0x00a9,0x0201,0x009a,0x00d4,0x2001,
	0x1001,0x0801,0x0401,0x0201,0x00b8,0x008b,0x0201,0x004d,0x00c7,0x0401,0x0201,
	0x007c,0x00d5,0x0201,0x005d,0x00e1,0x0801,0x0401,0x0201,0x001e,0x00e2,0x0201,
	0x00aa,0x00b9,0x0401,0x0201,0x009b,0x00e3,0x0201,0x00d6,0x006d,0x1401,0x0a01,
	0x0601,0x0201,0x003e,0x0201,0x002e,0x004e,0x0201,0x00c8,0x008c,0x0401,0x0201,
	0x00e4,0x00d7,0x0401,0x0201,0x007d,0x00ab,0x00e5,0x0a01,0x0401,0x0201,0x00ba,
	0x005e,0x0201,0x00c9,0x0201,0x009c,0x006e,0x0801,0x0201,0x00e6,0x0201,0x000d,
	0x0201,0x00e0,0x000e,0x0401,0x0201,0x00d8,0x008d,0x0201,0x00bb,0x00ca,0x4a01,
	0x0201,0x00ff,0x4001,0x3a01,0x2001,0x1001,0x0801,0x0401,0x0201,0x00ac,0x00e7,
	0x0201,0x007e,0x00d9,0x0401,0x0201,0x009d,0x00e8,0x0201,0x008e,0x00cb,0x0801,
	0x0401,0x0201,0x00bc,0x00da,0x0201,0x00ad,0x00e9,0x0401,0x0201,0x009e,0x00cc,
	0x0201,0x00db,0x00bd,0x1001,0x0801,0x0401,0x0201,0x00ea,0x00ae,0x0201,0x00dc,
	0x00cd,0x0401,0x0201,0x00eb,0x00be,0x0201,0x00dd,0x00ec,0x0801,0x0401,0x0201,
	0x00ce,0x00ed,0x0201,0x00de,0x00ee,0x000f,0x0401,0x0201,0x00f0,0x001f,0x00f1,
	0x0401,0x0201,0x00f2,0x002f,0x0201,0x00f3,0x003f,0x1201,0x0801,0x0401,0x0201,
	0x00f4,0x004f,0x0201,0x00f5,0x005f,0x0401,0x0201,0x00f6,0x006f,0x0201,0x00f7,
	0x0201,0x007f,0x008f,0x0a01,0x0401,0x0201,0x00f8,0x00f9,0x0401,0x0201,0x009f,
	0x00af,0x00fa,0x0801,0x0401,0x0201,0x00fb,0x00bf,0x0201,0x00fc,0x00cf,0x0401,
	0x0201,0x00fd,0x00df,0x0201,0x00fe,0x00ef,
	//},g_huffman_table_32[31] = {
	0x0201,0x0000,0x0801,0x0401,0x0201,0x0008,0x0004,0x0201,0x0001,0x0002,0x0801,
	0x0401,0x0201,0x000c,0x000a,0x0201,0x0003,0x0006,0x0601,0x0201,0x0009,0x0201,
	0x0005,0x0007,0x0401,0x0201,0x000e,0x000d,0x0201,0x000f,0x000b,
	//},g_huffman_table_33[31] = {
	0x1001,0x0801,0x0401,0x0201,0x0000,0x0001,0x0201,0x0002,0x0003,0x0401,0x0201,
	0x0004,0x0005,0x0201,0x0006,0x0007,0x0801,0x0401,0x0201,0x0008,0x0009,0x0201,
	0x000a,0x000b,0x0401,0x0201,0x000c,0x000d,0x0201,0x000e,0x000f,
};

const OpenMP3::HuffmanTable OpenMP3::kHuffmanTables[34] =
{
	{ NULL                  ,  0, 0 },  /* Table  0 */
	{ OpenMP3::kHuffmanTableSet       ,  7, 0 },  /* Table  1 */
	{ OpenMP3::kHuffmanTableSet + 7    , 17, 0 },  /* Table  2 */
	{ OpenMP3::kHuffmanTableSet + 24   , 17, 0 },  /* Table  3 */
	{ NULL                  ,  0, 0 },  /* Table  4 */
	{ OpenMP3::kHuffmanTableSet + 41   , 31, 0 },  /* Table  5 */
	{ OpenMP3::kHuffmanTableSet + 72   , 31, 0 },  /* Table  6 */
	{ OpenMP3::kHuffmanTableSet + 103  , 71, 0 },  /* Table  7 */
	{ OpenMP3::kHuffmanTableSet + 174  , 71, 0 },  /* Table  8 */
	{ OpenMP3::kHuffmanTableSet + 245  , 71, 0 },  /* Table  9 */
	{ OpenMP3::kHuffmanTableSet + 316  ,127, 0 },  /* Table 10 */
	{ OpenMP3::kHuffmanTableSet + 443  ,127, 0 },  /* Table 11 */
	{ OpenMP3::kHuffmanTableSet + 570  ,127, 0 },  /* Table 12 */
	{ OpenMP3::kHuffmanTableSet + 697  ,511, 0 },  /* Table 13 */
	{ NULL                  ,  0, 0 },  /* Table 14 */
	{ OpenMP3::kHuffmanTableSet + 1208 ,511, 0 },  /* Table 15 */
	{ OpenMP3::kHuffmanTableSet + 1719 ,511, 1 },  /* Table 16 */
	{ OpenMP3::kHuffmanTableSet + 1719 ,511, 2 },  /* Table 17 */
	{ OpenMP3::kHuffmanTableSet + 1719 ,511, 3 },  /* Table 18 */
	{ OpenMP3::kHuffmanTableSet + 1719 ,511, 4 },  /* Table 19 */
	{ OpenMP3::kHuffmanTableSet + 1719 ,511, 6 },  /* Table 20 */
	{ OpenMP3::kHuffmanTableSet + 1719 ,511, 8 },  /* Table 21 */
	{ OpenMP3::kHuffmanTableSet + 1719 ,511,10 },  /* Table 22 */
	{ OpenMP3::kHuffmanTableSet + 1719 ,511,13 },  /* Table 23 */
	{ OpenMP3::kHuffmanTableSet + 2230 ,512, 4 },  /* Table 24 */
	{ OpenMP3::kHuffmanTableSet + 2230 ,512, 5 },  /* Table 25 */
	{ OpenMP3::kHuffmanTableSet + 2230 ,512, 6 },  /* Table 26 */
	{ OpenMP3::kHuffmanTableSet + 2230 ,512, 7 },  /* Table 27 */
	{ OpenMP3::kHuffmanTableSet + 2230 ,512, 8 },  /* Table 28 */
	{ OpenMP3::kHuffmanTableSet + 2230 ,512, 9 },  /* Table 29 */
	{ OpenMP3::kHuffmanTableSet + 2230 ,512,11 },  /* Table 30 */
	{ OpenMP3::kHuffmanTableSet + 2230 ,512,13 },  /* Table 31 */
	{ OpenMP3::kHuffmanTableSet + 2742 , 31, 0 },  /* Table 32 */
	{ OpenMP3::kHuffmanTableSet + 2273 , 31, 0 },  /* Table 33 */
};
