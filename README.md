# game_of_life

 *A proof of concept of performant Game of Life implementation.*

 We represent cells as single bits and use clever bit shifts to effectlively achieve 
 21 wide parallelism within a single u64 number. This is more or less improvised version of SIMD.
 (in the old days of programming this was more common thing to do).
 We try to operate on as many bits as possible simultanously. How much is that? 
 We will answer indirectly. How many bits do we need to represent the entire GOL algorhitm? 
 (yes specifically GOL and NOT *any* automata of certain kind). 
 
 It seems we need 4. We need to sum all adjecent pixels and then do simple comparison of 
 the final value to determin if the cell lives. We need 4 bits because the maximum sum can be 8 (0b1000). 
 However if we discard all values higher than 7 as those will result in dead cell anyway we need just 3!
 (we apply some clever masking to achieve this, even if a bit tricky its fairly straigtforward).

 Okay so we need 3 bits to do the calculation. Now how does the actual algorhitm work. Lets explain this with a pictures.
 We are trying to determine if the middle cell lives or dies. 
 ```
 0   1   0
   -----
 1 | 1 | 1
   -----
 0   0   1
 ```
 To do this we first sum along each rows separately: 
 ```
 0 + 1 + 0 = 1
   -----
 1 + 1 + 1 = 3
   -----
 0 + 0 + 1 = 1
 ```
 This can be achived fairly easily in binary by the following piece of code
 where oct is a pattern of bits spaced 3 appart.
 ```c
  u64 row = /* bits in a single row of our bitmap chunk*/;
	u64 oct = 01111111111111111111111; //..1001001 in binary
	u64 sum_every_3_bits = (oct & (row >> 0));
		+ (oct & (row >> 1));
		+ (oct & (row >> 2));
 ```
 Now all thats left to do is to sum vertically. Here however we need to be careful since we cannot sum all 3 rows due to overflow 
 out of our 3 bits. So instead we only sum the first two and then dermine if the next addition would overflow. 
 We then use this to detrmine if the cell is "overfull" that is if the sum is a number which kills the cell no matter the circumstance.

 We then test if the bit patterns exactly match certain values (this can be done in effect with the following):
 ```c
  u64 pattern = /* bit pattern of 5 repeating */
  u64 row = /* bits in a single row of our bitmap chunk*/;
  u64 xored = pattern ^ row; //if are equl their XOR is everywhere 0
  u64 are_every_3_bits_not_equal_to_5 = (oct & (xored >> 0));
		| (oct & (xored >> 1));
		| (oct & (xored >> 2));

 /* now are_every_3_bits_not_equal_to_5 contains a set bit at the lowest adress in each oct (bit triplet) */
 ```
 Then we can test for equality simply like so:
 ```c
  u64 are_every_3_bits_equal_to_5 = ~are_every_3_bits_not_equal_to_5;
 ```
 Then we simply compose the necessary tests together using bitwise OR, AND and NEG and we recieved every 3rd bit of the output!
 We repeat this exact same procedure except with everything shifted down a bit to recieve every 3rd bit in the second set of the output.
 then finally the thir as well. We or these together and this yields the final value.
 
 This might look daunting (and indeed its painful to debug) but with a bit of patience its not that diffuclt to trial and error into.

 This approach is ~200x fatster than naive implementation using serial ifs, and about ~20x faster than branchless version. It can further be sped up about 4x
 by using 256 bit SIMD registers and doing essentially the same thing wider (or the exact same thing on every 1st 4th 8th 12th row simulatneously). 
 I was however lazy to implement this as this was fast enough for my need as the current bottle neck is rendering and then hash map lookup.
