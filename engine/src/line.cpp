/* Copyright (C) 2003-2013 Runtime Revolution Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "parsedef.h"
#include "objdefs.h"

#include "field.h"
#include "paragraf.h"
#include "MCBlock.h"
#include "line.h"
#include "context.h"
#include "uidc.h"

#include "globals.h"

MCLine::MCLine(MCParagraph *paragraph)
{
	parent = paragraph;
	firstblock = lastblock = NULL;
	width = ascent = descent = 0;
	dirtywidth = 0;
}

MCLine::~MCLine()
{
}

void MCLine::takebreaks(MCLine *lptr)
{
	if (firstblock != lptr->firstblock)
	{
		dirtywidth = MCU_max(width, lptr->width);
		firstblock = lptr->firstblock;
	}
	if (lastblock != lptr->lastblock)
	{
		dirtywidth = MCU_max(width, lptr->width);
		lastblock = lptr->lastblock;
	}
	if (width != lptr->width)
	{
		dirtywidth = MCU_max(width, lptr->width);
		width = lptr->width;
	}
	ascent = lptr->ascent;
	descent = lptr->descent;
	lptr->ascent = lptr->descent = 0;
	lptr->firstblock = lptr->lastblock = NULL;
	lptr->width = 0;
}

MCBlock *MCLine::fitblocks(MCBlock* p_first, MCBlock* p_sentinal, uint2 p_max_width)
{
	MCBlock *t_block;
	t_block = p_first;
	
	int4 t_frontier_width;
	t_frontier_width = 0;

	MCBlock *t_break_block;
	t_break_block = NULL;

	findex_t t_break_index;
	t_break_index = 0;

	do
	{
		t_block -> reset();

		// MW-2008-07-08: [[ Bug 6475 ]] Breaking works incorrectly on lines with tabs and multiple blocks.
		//   This is due to the tab computations within the block being incorrect as t_frontier_width was
		//   not being passed (width == 0 was being passed instead).
		findex_t t_new_break_index;
		bool t_break_fits, t_block_fits;
		t_block_fits = t_block -> fit(t_frontier_width, p_max_width < t_frontier_width ? 0 : p_max_width - t_frontier_width, t_new_break_index, t_break_fits);

		bool t_continue;
		if (t_block_fits)
		{
			if (t_new_break_index > t_block -> GetOffset() || (t_block -> GetLength() == 0 && t_break_fits))
			{
				// The whole block fits, so record the break position
				t_break_block = t_block;
				t_break_index = t_new_break_index;
			}
			else
			{
				// There is no break, so we need to look ahead to find a block
				// with a break
			}

			t_continue = true;			
		}
		else
		{
			if (t_new_break_index > t_block -> GetOffset())
			{
				// We have a break position but the whole block doesn't fit
				
				if (t_break_fits)
				{
					// The break fits, but the block doesn't so finish
					t_break_block = t_block;
					t_break_index = t_new_break_index;
					t_continue = false;
				}
				else if (t_break_block != NULL)
				{
					// The break doesn't fit and we've seen a break before
					t_continue = false;
				}
				else
				{
					// The break doesn't fit and we've not seen a break before
					t_break_block = t_block;
					t_break_index = t_new_break_index;
					t_continue = false;
				}
			}
			else
			{
				// We have no break position and the block doesn't fit
				
				if (t_break_block != NULL)
				{
					// The block doesn't fit and we've seen a break before
					t_continue = false;
				}
				else
				{
					// We have no previous break and the block doesn't fit
					// continue until we get to a break or the last block
					t_continue = true;
				}
			}
		}
				
		if (!t_continue)
			break;
			
		t_frontier_width += t_block -> getwidth(NULL, t_frontier_width);
		
		t_block = t_block -> next();
	}
	while(t_block != p_sentinal);
	
	if (t_break_block == NULL)
	{
		t_break_block = t_block -> prev();
		t_break_index = t_break_block -> GetOffset() + t_break_block -> GetLength();
	}
	
	// MW-2014-01-06: [[ Bug 11628 ]] We have a break block and index, so now extend the
	//   break index through any space chars.
	for(;;)
	{
		// Consume all spaces after the break index.
		while(t_break_index < (t_break_block -> GetOffset() + t_break_block -> GetLength()) &&
			  parent -> TextIsWordBreak(t_break_block -> GetCodepointAtIndex(t_break_index)))
			t_break_index++;
		
		if (t_break_index < (t_break_block -> GetOffset() + t_break_block -> GetLength()))
			break;

        // Get the next non empty block.
        MCBlock *t_next_block;
        t_next_block = t_break_block -> next();
        while(t_next_block -> GetLength() == 0 && t_next_block != p_sentinal)
            t_next_block = t_next_block -> next();

		// If we are at the end of the list of blocks there is nothing more to do.
		if (t_next_block == p_sentinal)
			break;
		
		// If the first char of the next block is not a space, then there is nothing more
		// to do.
		if (!parent -> TextIsWordBreak(t_next_block -> GetCodepointAtIndex(t_break_index)))
			break;
		
		// The next block starts with a space, so advance the break block.
		t_break_block = t_next_block;
		t_break_index = t_break_block -> GetOffset();
	}
	
	// MW-2012-02-21: [[ LineBreak ]] Check to see if there is a vtab char before the
	//   break index.
	bool t_is_explicit_line_break;
	t_is_explicit_line_break = false;
	t_block = p_first;
	do
	{
		findex_t t_line_break_index;
		if (t_block -> GetFirstLineBreak(t_line_break_index))
		{
			// If the explicit line break is the same as the break, then make sure we
			// mark it as explicit so that line breaks at ends of lines work.
			if (t_line_break_index <= t_break_index)
			{
				t_is_explicit_line_break = true;
				t_break_index = t_line_break_index;
				t_break_block = t_block;
			}
			break;
		}
		t_block = t_block -> next();
	}
	while(t_block != t_break_block -> next());
	
	// If the break index is before the end of the block *or* if we are explicit and it
	// is at the end of the block, split the block. [ The latter rule means there is an
	// empty block to have as a line ].
	if (t_break_index < t_break_block -> GetOffset() + t_break_block -> GetLength() ||
		(t_is_explicit_line_break && t_break_index == t_break_block -> GetOffset() + t_break_block -> GetLength()))
		t_break_block -> split(t_break_index);
		
	firstblock = p_first;
	lastblock = t_break_block;
	
	ResolveDisplayOrder();
	
	dirtywidth = width;
	
	return lastblock -> next();
}

void MCLine::appendall(MCBlock *bptr)
{
	firstblock = bptr;
	lastblock = (MCBlock *)bptr->prev();
	uint2 oldwidth = width;
	width = 0;
	bptr = lastblock;
	ascent = descent = 0;
    ResolveDisplayOrder();
	/*do
	{
		// TODO: RTL support
        
        bptr = (MCBlock *)bptr->next();
		setscents(bptr);
        bptr -> setorigin(width);
		width += bptr->getwidth(NULL, width);
	}
	while (bptr != lastblock);*/
	dirtywidth = MCU_max(width, oldwidth);
}

void MCLine::draw(MCDC *dc, int2 x, int2 y, findex_t si, findex_t ei, MCStringRef p_string, uint2 pstyle)
{
	//int2 cx = 0;
	MCBlock *bptr = (MCBlock *)firstblock->prev();
	
	uint32_t t_flags;
	t_flags = 0;
	
	bool t_is_flagged;
	t_is_flagged = false;
	int32_t t_flagged_sx, t_flagged_ex;
	t_flagged_sx = 0;
	t_flagged_ex = 0;
	do
	{
		bptr = (MCBlock *)bptr->next();
		
		// MW-2012-02-27: [[ Bug 2939 ]] We need to compute whether to render the left/right
		//   edge of the box style. This is determined by whether the previous block has such
		//   a style or not.
		
		// Fetch the style of this block.
		uint2 t_this_style;
		if (!bptr -> gettextstyle(t_this_style))
			t_this_style = pstyle;
		
		// Start off with 0 flags.
		t_flags = 0;
		
		// If this block has a box style then we have something to do.
		if ((t_this_style & (FA_BOX | FA_3D_BOX)) != 0)
		{
			// If we are not the first block, check the text style of the previous one.
			// Otherwise we must render the left edge.
			if (bptr != firstblock)
			{
				uint2 t_prev_style;
				if (!bptr -> prev() -> gettextstyle(t_prev_style))
					t_prev_style = pstyle;
				if ((t_this_style & FA_BOX) != 0 && (t_prev_style & FA_BOX) == 0)
					t_flags |= DBF_DRAW_LEFT_EDGE;
				else if ((t_this_style & FA_3D_BOX) != 0 && (t_prev_style & FA_3D_BOX) == 0)
					t_flags |= DBF_DRAW_LEFT_EDGE;
			}
			else
				t_flags |= DBF_DRAW_LEFT_EDGE;
			
			// If we are not the last block, check the text style of the next one.
			// Otherwise we must render the right edge.
			if (bptr != lastblock)
			{
				uint2 t_next_style;
				if (!bptr -> next() -> gettextstyle(t_next_style))
					t_next_style = pstyle;
				if ((t_this_style & FA_BOX) != 0 && (t_next_style & FA_BOX) == 0)
					t_flags |= DBF_DRAW_RIGHT_EDGE;
				else if ((t_this_style & FA_3D_BOX) != 0 && (t_next_style & FA_3D_BOX) == 0)
					t_flags |= DBF_DRAW_RIGHT_EDGE;
			}
			else
				t_flags |= DBF_DRAW_RIGHT_EDGE;
		}
		
		// Pass the computed flags to the block to draw.
		bptr->draw(dc, x + bptr->getorigin(), bptr->getorigin(), y, si, ei, p_string, pstyle, t_flags);
		
		uint2 twidth;
		twidth = bptr->getwidth(dc);
		
		if (bptr -> getflagged())
		{
			if (!t_is_flagged)
			{
				t_is_flagged = true;
				t_flagged_sx = x;
			}
			t_flagged_ex = x + twidth;
		}
		
		if (t_is_flagged && (!bptr -> getflagged() || bptr == lastblock))
		{
			static uint1 s_dashes[2] = {3, 2};
			MCColor t_color;
			t_color . red = 255 << 8;
			t_color . green = 0 << 8;
			t_color . blue = 0 << 8;
			MCscreen -> alloccolor(t_color);
			dc -> setforeground(t_color);
			dc -> setquality(QUALITY_SMOOTH);
			dc -> setlineatts(2, LineOnOffDash, CapButt, JoinRound);
			dc -> setdashes(0, s_dashes, 2);
			dc -> drawline(t_flagged_sx, y + 1, t_flagged_ex, y + 1);
			dc -> setlineatts(0, LineSolid, CapButt, JoinBevel);
			dc -> setquality(QUALITY_DEFAULT);
			t_is_flagged = false;
			parent->getparent()->setforeground(dc, DI_FORE, False, True);
		}
		
		//x += twidth;
		//cx += twidth;
	}
	while (bptr != lastblock);
}

void MCLine::setscents(MCBlock *bptr)
{
	uint2 aheight = bptr->getascent();
	if (aheight > ascent)
		ascent = aheight;
	uint2 dheight = bptr->getdescent();
	if (dheight > descent)
		descent = dheight;
}

uint2 MCLine::getdirtywidth()
{
	return dirtywidth;
}

void MCLine::clean()
{
	dirtywidth = 0;
}

void MCLine::makedirty()
{
	dirtywidth = MCU_max(width, 1);
}

void MCLine::GetRange(findex_t &i, findex_t &l)
{
	firstblock->GetRange(i, l);
	findex_t j;
	lastblock->GetRange(j, l);
	l = j + l - i;
}

uint2 MCLine::GetCursorXHelper(findex_t fi, bool prefer_forward)
{
    MCBlock *bptr = (MCBlock *)firstblock;
	findex_t i, l;
	bptr->GetRange(i, l);
    
    while (bptr != lastblock
           && ((prefer_forward && fi >= i + l)
           || (!prefer_forward && fi >  i + l)))
    {
        bptr = bptr -> next();
        bptr -> GetRange(i, l);
    }
   
    return bptr->GetCursorX(fi);
}

uint2 MCLine::GetCursorXPrimary(findex_t fi, bool moving_forward)
{
	return GetCursorXHelper(fi, !moving_forward);
}

uint2 MCLine::GetCursorXSecondary(findex_t fi, bool moving_forward)
{
    return GetCursorXHelper(fi, moving_forward);
}

findex_t MCLine::GetCursorIndex(int2 cx, Boolean chunk, bool moving_left)
{
	/*uint2 x = 0;
	MCBlock *bptr = firstblock;
	int2 bwidth = bptr->getwidth(NULL, x);
	while (cx > bwidth && bptr != lastblock)
	{
		cx -= bwidth;
		x += bwidth;
		bptr = (MCBlock *)bptr->next();
		bwidth = bptr->getwidth(NULL, x);
	}

	return bptr->GetCursorIndex(x, cx, chunk, bptr == lastblock);*/
    
    // BIDIRECTIONAL SUPPORT -
    //  Blocks cannot be assumed to be in visual order
    
    // TODO: when cx > line width, return the last block in visual order
    
    MCBlock *bptr = firstblock;
    while (bptr != lastblock)
    {
        if (cx >= (int4)bptr->getorigin() && cx < (int4)(bptr->getorigin() + bptr->getwidth()))
            break;
        
        bptr = bptr->next();
    }
        
    return bptr->GetCursorIndex(cx, chunk, bptr == lastblock);
}

uint2 MCLine::getwidth()
{
	return width;
}

uint2 MCLine::getheight()
{
	return ascent + descent;
}

uint2 MCLine::getascent()
{
	return ascent;
}

uint2 MCLine::getdescent()
{
	return descent;
}

// MW-2012-02-10: [[ FixedTable ]] In fixed-width table mode the paragraph needs to
//   explicitly set the width of the table.
void MCLine::setwidth(uint2 p_new_width)
{
	width = p_new_width;
}

void MCLine::ResolveDisplayOrder()
{
    // Count the number of blocks in the line
    uindex_t t_block_count;
    t_block_count = 1;
    MCBlock *bptr;
    bptr = firstblock;
    while (bptr != lastblock)
    {
        t_block_count++;
        bptr = bptr->next();
    }
    
    // Create an array to store the blocks in visual order
    MCAutoArray<MCBlock *> t_visual_order;
    /* UNCHECKED */ t_visual_order.New(t_block_count);
    
    // Initialise the visual order to be the logical order. Also take this
    // opportunity to find the highest and lowest direction level in the line.
    uint8_t t_max_level, t_min_level;
    t_max_level = 0;
    t_min_level = 255;
    bptr = firstblock;
    for (uindex_t i = 0; i < t_block_count; i++)
    {
        uint8_t t_level;
        t_level = bptr->GetDirectionLevel();
        if (t_level > t_max_level)
            t_max_level = t_level;
        
        if (t_level < t_min_level)
            t_min_level = t_level;
        
        t_visual_order[i] = bptr;
        bptr = bptr->next();
    }
    
    // Unicode Bidi Algorithm rule L2:
    //  "From the highest level found in the text to the lowest odd level on each line,
    //   including intermediate levels not actually present in the text, reverse any
    //   contiguous sequence of characters that are at that level or higher."
    //
    // We don't actually reverse the text here, just the ordering of the blocks.
    // To avoid creating a new stringref, a simple flag is used to indicate reversal.
    
    // Adjust the min level to be the minimum odd level
    t_min_level += (t_min_level & 1) ? 0 : 1;
    
    for (uindex_t i = t_max_level; i >= t_min_level; i--)
    {
        // Scan the block list for a block at this level or higher
        uindex_t j = 0;
        while (j < t_block_count)
        {
            // Not at the desired level; move to the next block
            if (t_visual_order[j]->GetDirectionLevel() < i)
            {
                j++;
                continue;
            }
            
            // Scan for the last contigious block at or above this level
            uindex_t t_length;
            t_length = 0;
            while (j + t_length < t_block_count && t_visual_order[j + t_length]->GetDirectionLevel() >= i)
                t_length++;
            
            // Reverse the affected section of the visual order array
            uindex_t t_even, t_pivot, t_stride;
            t_even = 1 - (t_length & 1);
            t_stride = t_length >> 1;
            t_pivot = j + t_stride;
            while (t_stride > 0)
            {
                uindex_t t_low, t_high;
                t_low = t_pivot - t_stride;
                t_high = t_pivot + t_stride - t_even;
                
                // TODO: toggle the "reversed" flag on the block or is the encoding enough?
                
                MCBlock *temp = t_visual_order[t_low];
                t_visual_order[t_low] = t_visual_order[t_high];
                t_visual_order[t_high] = temp;
                
                t_stride--;
            }
            
            // This run of blocks has been reversed
            j += t_length;
        }
    }
    
    // The blocks are now in visual order. Calculate their positions (and also
    // the width of this line). A second pass will be needed to resolve the
    // offsets to be used when calculating tabstops.
    for (uindex_t i = 0; i < t_block_count; i++)
    {
        bptr = t_visual_order[i];
        setscents(bptr);
        bptr -> setorigin(width);
        bptr -> SetVisualIndex(i);
        width += bptr -> getwidth(NULL);
    }
    
    // Tabs are always done using the dominant paragraph direction
    bool t_para_is_rtl;
    t_para_is_rtl = parent->getbasetextdirection() == kMCFieldTextDirectionRTL;
    
    uint2 t_tabpos = (t_para_is_rtl) ? width : 0;
    
    // Tell each block what pixel offset it should use when calculating tabstops
    for (uindex_t i = 0; i < t_block_count; i++)
    {
        bptr -> settabpos(t_tabpos);
        if (t_para_is_rtl)
            t_tabpos -= bptr->getwidth(NULL);
        else
            t_tabpos += bptr->getwidth(NULL);
    }
}
