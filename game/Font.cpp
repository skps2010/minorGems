#include "Font.h"

#include "minorGems/graphics/RGBAImage.h"

#include <string.h>
#include <assert.h>
#include <iostream>


typedef union rgbaColor {
        struct comp { 
                unsigned char r;
                unsigned char g;
                unsigned char b;
                unsigned char a;
            } comp;
        
        // access those bytes as an array
        unsigned char bytes[4];
        
        // reinterpret those bytes as an unsigned int
        unsigned int rgbaInt; 
    } rgbaColor;


// what alpha level counts as "ink" when measuring character width
// and doing kerning
// values at or below this level will not count as ink
// this improves kerning and font spacing, because dim "tips" of pointed
// glyphs don't cause the glyph to be logically wider than it looks visually 
const unsigned char inkA = 127;

unicode utf8ToCodepoint(const unsigned char *&p)
{
    unicode codepoint = 0;
    if ((*p & 0x80) == 0)
    {
        codepoint = *p;
        p++;
    }
    else if ((*p & 0xE0) == 0xC0)
    {
        codepoint = (*p & 0x1F) << 6;
        p++;
        codepoint |= (*p & 0x3F);
        p++;
    }
    else if ((*p & 0xF0) == 0xE0)
    {
        codepoint = (*p & 0x0F) << 12;
        p++;
        codepoint |= (*p & 0x3F) << 6;
        p++;
        codepoint |= (*p & 0x3F);
        p++;
    }
    else if ((*p & 0xF8) == 0xF0)
    {
        codepoint = (*p & 0x07) << 18;
        p++;
        codepoint |= (*p & 0x3F) << 12;
        p++;
        codepoint |= (*p & 0x3F) << 6;
        p++;
        codepoint |= (*p & 0x3F);
        p++;
    }
    else
    {
        std::cout << "Not UTF-8: " << (char)*p << ',' << (int)*p << ',' << std::endl;
    }
    return codepoint;
}

unicode *utf8ToUnicode(const char *utf8String)
{
    const unsigned char *p = (const unsigned char *)utf8String;
    unicode output[strlen(utf8String)];
    int len = 0;

    while (*p)
    {
        output[len] = utf8ToCodepoint(p);
        ++len;
    }

    unicode *result = new unicode[len + 1];
    for (int i = 0; i < len; ++i)
    {
        result[i] = output[i];
    }
    result[len] = 0;
    return result;
}

size_t strlen(const unicode *u)
{
    int i = 0;
    while (u[i])
        ++i;
    return i;
}

static Image *unicodeSpriteImage[256];

Font::Font( const char *inFileName, int inCharSpacing, int inSpaceWidth,
            char inFixedWidth, double inScaleFactor, int inFixedCharWidth )
        : mScaleFactor( inScaleFactor ),
          mCharSpacing( inCharSpacing ), mSpaceWidth( inSpaceWidth ),
          mFixedWidth( inFixedWidth ), mEnableKerning( true ),
          mMinimumPositionPrecision( 0 ) {

    for( int i=0; i<256; i++ ) {
        mSpriteMap[i] = NULL;
        mKerningTable[i] = NULL;
    }


    Image *spriteImage = readTGAFile( inFileName );
    
    if( spriteImage != NULL ) {
        
        int width = spriteImage->getWidth();
        
        int height = spriteImage->getHeight();
        
        int numPixels = width * height;
        
        
            
        // hold onto these for true kerning after
        // we've read this data for all characters
        rgbaColor *savedCharacterRGBA[256];
        std::cout << "load font" << std::endl;
        
        for( int f=0; f<256; f++) {
            // read unicode font
            if(f > 0) {
                // unicode hasn't setup
                if(unicodeSpriteImage[f] == NULL) {
                    char filename[28];
                    sprintf(filename, "unicode/unicode_page_%02x.tga", f);
                    spriteImage = readTGAFile( filename );
                    if(spriteImage == NULL)
                        continue;

                    unicodeSpriteImage[f] = spriteImage;
                }
                else
                    spriteImage = unicodeSpriteImage[f];
            }
            mSpriteWidth = width / 16;
            mSpriteHeight = height / 16;

            int realWidth = spriteImage->getWidth();
            int realHeight = spriteImage->getHeight();
            int realSpriteWidth = realWidth / 16;
            int realSpriteHeight = realHeight / 16;

            rgbaColor *spriteRGBA = new rgbaColor[ numPixels ];
            unsigned char *spriteBytes = 
                RGBAImage::getRGBABytes( spriteImage );
            
            if(f == 0)
                delete spriteImage;

            int index = 0;
            for( int i=0; i<numPixels; i++ ) {
                // x, y in a char sprite
                int x = i % mSpriteWidth;
                int y = (i / width) % mSpriteHeight;
                
                for( int b=0; b<4; b++ ) {
                    
                    if(x >= realSpriteWidth || y < mSpriteHeight - realSpriteHeight)
                        // padding with black
                        spriteRGBA[i].bytes[b] = 0;
                    else {
                        spriteRGBA[i].bytes[b] = spriteBytes[ index ];
                        ++index;
                    }
                    }
                }
            assert(index == realWidth*realHeight*4);
            
            delete [] spriteBytes;
            
            

            // use red channel intensity as transparency
            // make entire image solid white and use transparency to 
            // mask it

            for( int i=0; i<numPixels; i++ ) {
                spriteRGBA[i].comp.a = spriteRGBA[i].comp.r;
                
                spriteRGBA[i].comp.r = 255;
                spriteRGBA[i].comp.g = 255;
                spriteRGBA[i].comp.b = 255;
                }
                
                            
            
            
            if( mSpriteHeight == mSpriteWidth ) {
                mAccentsPresent = false;
                }
            else {
                mAccentsPresent = true;
                }

            if( inFixedCharWidth == 0 ) {
                mCharBlockWidth = mSpriteWidth;
                }
            else {
                mCharBlockWidth = inFixedCharWidth;
                }


            int pixelsPerChar = mSpriteWidth * mSpriteHeight;

            for( int i=0; i<256; i++ ) {
                int yOffset = ( i / 16 ) * mSpriteHeight;
                int xOffset = ( i % 16 ) * mSpriteWidth;
                
                rgbaColor *charRGBA = new rgbaColor[ pixelsPerChar ];
                
                for( int y=0; y<mSpriteHeight; y++ ) {
                    for( int x=0; x<mSpriteWidth; x++ ) {
                        
                        int imageIndex = (y + yOffset) * width
                            + x + xOffset;
                        int charIndex = y * mSpriteWidth + x;
                        
                        charRGBA[ charIndex ] = spriteRGBA[ imageIndex ];
                        }
                    }
                    
                // don't bother consuming texture ram for blank sprites
                char allTransparent = true;
                
                for( int p=0; p<pixelsPerChar && allTransparent; p++ ) {
                    if( charRGBA[ p ].comp.a != 0 ) {
                        allTransparent = false;
                        }
                    }
                    
                if( !allTransparent ) {
                    
                    // convert into an image
                    Image *charImage = new Image( mSpriteWidth, mSpriteHeight,
                                                4, false );
                    
                    for( int c=0; c<4; c++ ) {
                        double *chan = charImage->getChannel(c);
                        
                        for( int p=0; p<pixelsPerChar; p++ ) {
                            
                            chan[p] = charRGBA[p].bytes[c] / 255.0;
                            }
                        }
                    

                    mSpriteMap[256 * f + i] = 
                        fillSprite( charImage );
                    delete charImage;
                    }
                else {
                    mSpriteMap[256 * f + i] = NULL;
                    }
                

                if( mFixedWidth ) {
                    mCharLeftEdgeOffset[256 * f + i] = 0;
                    mCharWidth[256 * f + i] = mCharBlockWidth;
                    }
                else if( allTransparent ) {
                    mCharLeftEdgeOffset[256 * f + i] = 0;
                    mCharWidth[256 * f + i] = mSpriteWidth;
                    }
                else {
                    // implement pseudo-kerning
                    
                    int farthestLeft = mSpriteWidth;
                    int farthestRight = 0;
                    
                    char someInk = false;
                    
                    for( int y=0; y<mSpriteHeight; y++ ) {
                        for( int x=0; x<mSpriteWidth; x++ ) {
                            
                            unsigned char a = 
                                charRGBA[ y * mSpriteWidth + x ].comp.a;
                            
                            if( a > inkA ) {
                                someInk = true;
                                
                                if( x < farthestLeft ) {
                                    farthestLeft = x;
                                    }
                                if( x > farthestRight ) {
                                    farthestRight = x;
                                    }
                                }
                            }
                        }
                    
                    if( ! someInk  ) {
                        mCharLeftEdgeOffset[256 * f + i] = 0;
                        mCharWidth[256 * f + i] = mSpriteWidth;
                        }
                    else {
                        mCharLeftEdgeOffset[256 * f + i] = farthestLeft;
                        mCharWidth[256 * f + i] = farthestRight - farthestLeft + 1;
                        }
                    }
                    

                if(f == 0) {
                    if( !allTransparent && ! mFixedWidth ) {
                        savedCharacterRGBA[i] = charRGBA;
                        }
                    else {
                        savedCharacterRGBA[i] = NULL;
                        delete [] charRGBA;
                        }
                }
            }
            delete [] spriteRGBA;
        }

        // now that we've read in all characters, we can do real kerning
        // unicodes don't use kerning, because it takes time
        if( !mFixedWidth ) {
            
            // first, compute left and right extremes for each pixel
            // row of each character
            int *rightExtremes[256];
            int *leftExtremes[256];
            
            for( int i=0; i<256; i++ ) {
                rightExtremes[i] = new int[ mSpriteHeight ];
                leftExtremes[i] = new int[ mSpriteHeight ];
                
                if( savedCharacterRGBA[i] != NULL ) {
                    for( int y=0; y<mSpriteHeight; y++ ) {
                        
                        int rightExtreme = 0;
                        int leftExtreme = mSpriteWidth;
                            
                        for( int x=0; x<mSpriteWidth; x++ ) {
                            int p = y * mSpriteWidth + x;
                                
                            if( savedCharacterRGBA[i][p].comp.a > inkA ) {
                                rightExtreme = x;
                                }
                            if( x < leftExtreme &&
                                savedCharacterRGBA[i][p].comp.a > inkA ) {
                                
                                leftExtreme = x;
                                }
                            // also check pixel rows above and below
                            // for left character, to look for
                            // diagonal collisions (perfect nesting
                            // with no vertical gap)
                            if( y > 0 && x < leftExtreme ) {
                                int pp = (y-1) * mSpriteWidth + x;
                                if( savedCharacterRGBA[i][pp].comp.a 
                                    > inkA ) {
                                    
                                    leftExtreme = x;
                                    }
                                }
                            if( y < mSpriteHeight - 1 
                                && x < leftExtreme ) {
                                
                                int pp = (y+1) * mSpriteWidth + x;
                                if( savedCharacterRGBA[i][pp].comp.a 
                                    > inkA ) {
                                    
                                    leftExtreme = x;
                                    }
                                }
                            }
                        
                        rightExtremes[i][y] = rightExtreme;
                        leftExtremes[i][y] = leftExtreme;
                        }
                    }
                }
            


            for( int i=0; i<256; i++ ) {
                if( savedCharacterRGBA[i] != NULL ) {
                
                    mKerningTable[i] = new KerningTable;


                    // for each character that could come after this character
                    for( int j=0; j<256; j++ ) {

                        mKerningTable[i]->offset[j] = 0;

                        // not a blank character
                        if( savedCharacterRGBA[j] != NULL ) {
                        
                            short minDistance = 2 * mSpriteWidth;

                            // for each pixel row, find distance
                            // between the right extreme of the first character
                            // and the left extreme of the second
                            for( int y=0; y<mSpriteHeight; y++ ) {
                            
                                int rightExtreme = rightExtremes[i][y];
                                int leftExtreme = leftExtremes[j][y];
                            
                                int rowDistance =
                                    ( mSpriteWidth - rightExtreme - 1 ) 
                                    + leftExtreme;

                                if( rowDistance < minDistance ) {
                                    minDistance = rowDistance;
                                    }
                                }
                        
                            // have min distance across all rows for 
                            // this character pair

                            // of course, we've already done pseudo-kerning
                            // based on character width, so take that into 
                            // account
                            // 
                            // true kerning is a tweak to that
                        
                            // pseudo-kerning already accounts for
                            // gap to left of second character
                            minDistance -= mCharLeftEdgeOffset[j];
                            // pseudo-kerning already accounts for gap to right
                            // of first character
                            minDistance -= 
                                mSpriteWidth - 
                                ( mCharLeftEdgeOffset[i] + mCharWidth[i] );
                        
                            if( minDistance > 0 
                                // make sure we don't have a full overhang
                                // for characters that don't collide
                                // horizontally at all
                                && minDistance < mCharWidth[i] ) {
                            
                                mKerningTable[i]->offset[j] = - minDistance;
                                }
                            }
                        }
                
                    }
                }

            for( int i=0; i<256; i++ ) {
                delete [] rightExtremes[i];
                delete [] leftExtremes[i];
                }
            }

        for( int i=0; i<256; i++ ) {
            if( savedCharacterRGBA[i] != NULL ) {
                delete [] savedCharacterRGBA[i];
                }
            }
        }
    }



Font::~Font() {
    for( int i=0; i<65536; i++ ) {
        if( mSpriteMap[i] != NULL ) {
            freeSprite( mSpriteMap[i] );
            }
        if( i < 256 && mKerningTable[i] != NULL ) {
            delete mKerningTable[i];
            }
        }
    }



void Font::copySpacing( Font *inOtherFont ) {
    memcpy( mCharLeftEdgeOffset, inOtherFont->mCharLeftEdgeOffset,
            65536 * sizeof( int ) );

    memcpy( mCharWidth, inOtherFont->mCharWidth,
            65536 * sizeof( int ) );
    

    for( int i=0; i<256; i++ ) {
        if( mKerningTable[i] != NULL ) {
            delete mKerningTable[i];
            mKerningTable[i] = NULL;
            }

        if( inOtherFont->mKerningTable[i] != NULL ) {
            mKerningTable[i] = new KerningTable;
            memcpy( mKerningTable[i]->offset,
                    inOtherFont->mKerningTable[i]->offset,
                    256 * sizeof( short ) );
            }
        }

    mScaleFactor = inOtherFont->mScaleFactor;
        
    
    mCharSpacing = inOtherFont->mCharSpacing;
    mSpaceWidth = inOtherFont->mSpaceWidth;
        
    mFixedWidth = inOtherFont->mFixedWidth;
        
    mSpriteWidth = inOtherFont->mSpriteWidth;
    mSpriteHeight = inOtherFont->mSpriteHeight;
    
    mAccentsPresent = inOtherFont->mAccentsPresent;
        

    mCharBlockWidth = inOtherFont->mCharBlockWidth;
    }



// double pixel size
static double scaleFactor = 1.0 / 16;
//static double scaleFactor = 1.0 / 8;



double Font::getCharSpacing() {
    double scale = scaleFactor * mScaleFactor;
    
    return mCharSpacing * scale;
    }


double Font::getCharPos( SimpleVector<doublePair> *outPositions,
                         const char *inString, doublePair inPosition,
                         TextAlignment inAlign ) {
    unicode *unicodeString = utf8ToUnicode(inString);
    double result = getCharPos(outPositions, unicodeString, inPosition, inAlign);
    delete[] unicodeString;
    return result;
}
double Font::getCharPos( SimpleVector<doublePair> *outPositions,
                         const unicode *inString, doublePair inPosition,
                         TextAlignment inAlign ) {

    double scale = scaleFactor * mScaleFactor;
    
    unsigned int numChars = strlen( inString );
    
    double x = inPosition.x;
    
    
    double y = inPosition.y;

    // compensate for extra headspace in accent-equipped font files
    if( mAccentsPresent ) { 
        y += scale * mSpriteHeight / 4;
        }

    
    double stringWidth = 0;
    
    if( inAlign != alignLeft ) {
        stringWidth = measureString( inString );
        }
    
    switch( inAlign ) {
        case alignCenter:
            x -= stringWidth / 2;
            break;
        case alignRight:
            x -= stringWidth;
            break;
        default:
            // left?  do nothing
            break;            
        }
    
    // character sprites are drawn on their centers, so the alignment
    // adjustments above aren't quite right.
    x += scale * mSpriteWidth / 2;


    if( mMinimumPositionPrecision > 0 ) {
        x /= mMinimumPositionPrecision;
        
        x = lrint( floor( x ) );
        
        x *= mMinimumPositionPrecision;
        }
    

    for( unsigned int i=0; i<numChars; i++ ) {
        doublePair charPos = { x, y };
        
        doublePair drawPos;
        
        double charWidth = positionCharacter( inString[i], 
                                              charPos, &drawPos );
        outPositions->push_back( drawPos );
        
        x += charWidth + mCharSpacing * scale;
        
        if( !mFixedWidth && mEnableKerning 
            && i < numChars - 1 
            && inString[i] < 256
            && inString[i+1] < 256
            && mKerningTable[ inString[i] ] != NULL ) {
            // there's another character after this
            // apply true kerning adjustment to the pair
            int offset = mKerningTable[ inString[i] ]->
                offset[ inString[i+1] ];
            x += offset * scale;
            }
        }
    // no spacing after last character
    x -= mCharSpacing * scale;

    return x;
    }




double Font::drawString( const char *inString, doublePair inPosition,
                         TextAlignment inAlign ) {
    unicode* unicodeString = utf8ToUnicode(inString);
    SimpleVector<doublePair> pos( strlen( unicodeString ) );

    double returnVal = getCharPos( &pos, unicodeString, inPosition, inAlign );

    double scale = scaleFactor * mScaleFactor;
    
    for( int i=0; i<pos.size(); i++ ) {
        SpriteHandle spriteID = mSpriteMap[ unicodeString[i] ];
    
        if( spriteID != NULL ) {
            drawSprite( spriteID, pos.getElementDirect(i), scale );
            }
    
        }
    
    delete[] unicodeString;
    return returnVal;
    }




double Font::positionCharacter( unicode inC, doublePair inTargetPos,
                                doublePair *outActualPos ) {
    *outActualPos = inTargetPos;
    
    double scale = scaleFactor * mScaleFactor;

    if( inC == ' ' ) {
        return mSpaceWidth * scale;
        }

    if( !mFixedWidth ) {
        outActualPos->x -= mCharLeftEdgeOffset[ inC ] * scale;
        }
    
    if( mFixedWidth ) {
        return mCharBlockWidth * scale;
        }
    else {
        return mCharWidth[ inC ] * scale;
        }
    }

    


double Font::drawCharacter( unicode inC, doublePair inPosition ) {
    
    doublePair drawPos;
    double returnVal = positionCharacter( inC, inPosition, &drawPos );

    if( inC == ' ' ) {
        return returnVal;
        }

    SpriteHandle spriteID = mSpriteMap[ inC ];
    
    if( spriteID != NULL ) {
        double scale = scaleFactor * mScaleFactor;
        drawSprite( spriteID, drawPos, scale );
        }
    
    return returnVal;
    }



void Font::drawCharacterSprite( unicode inC, doublePair inPosition ) {
    SpriteHandle spriteID = mSpriteMap[ inC ];
    
    if( spriteID != NULL ) {
        double scale = scaleFactor * mScaleFactor;
        drawSprite( spriteID, inPosition, scale );
        }
    }

double Font::measureString( const char *inString, int inCharLimit ) {
    unicode* unicodeString = utf8ToUnicode(inString);
    double result =  measureString(unicodeString, inCharLimit);
    delete[] unicodeString;
    return result;
}
double Font::measureString( const unicode *inString, int inCharLimit ) {
    double scale = scaleFactor * mScaleFactor;

    int numChars = inCharLimit;

    if( numChars == -1 ) {
        // no limit, measure whole string
        numChars = strlen( inString );
        }
    
    double width = 0;
    
    for( int i=0; i<numChars; i++ ) {
        unicode c = inString[i];
        
        if( c == ' ' ) {
            width += mSpaceWidth * scale;
            }
        else if( mFixedWidth ) {
            width += mCharBlockWidth * scale;
            }
        else {
            width += mCharWidth[ c ] * scale;

            if( mEnableKerning
                && i < numChars - 1
                && inString[i] < 256
                && inString[i+1] < 256
                && mKerningTable[ inString[i] ] != NULL ) {
                // there's another character after this
                // apply true kerning adjustment to the pair
                int offset = mKerningTable[ inString[i] ]->
                    offset[ inString[i+1] ];
                width += offset * scale;
                }
            }
    
        width += mCharSpacing * scale;
        }

    if( numChars > 0 ) {    
        // no extra space at end
        // (added in last step of loop)
        width -= mCharSpacing * scale;
        }

    return width;
    }



double Font::getFontHeight() {
    double accentFactor = 1.0f;
    
    if( mAccentsPresent ) {
        accentFactor = 0.5f;
        }
    
    return scaleFactor * mScaleFactor * mSpriteHeight * accentFactor;
    }



void Font::enableKerning( char inKerningOn ) {
    mEnableKerning = inKerningOn;
    }



void Font::setMinimumPositionPrecision( double inMinimum ) {
    mMinimumPositionPrecision = inMinimum;
    }



