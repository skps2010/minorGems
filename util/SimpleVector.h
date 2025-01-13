// Jason Rohrer
// SimpleVector.h

/**
*
*	Simple vector template class. Supports pushing at end and random-access deletions.
*	Dynamically sized.
*
*
*	Created 10-24-99
*	Mods:
*		Jason Rohrer	12-11-99	Added deleteAll function
*		Jason Rohrer	1-30-2000	Changed to return NULL if get called on non-existent element
*		Jason Rohrer	12-20-2000	Added a function for deleting a particular
*									element.
*		Jason Rohrer	12-14-2001	Added a function for getting the index of
*									a particular element.
*		Jason Rohrer	1-24-2003	Added a functions for getting an array or
*									string of all elements.
*		Jason Rohrer	7-26-2005	Added template <> to explicitly specialized
*									getElementString.
*		Jason Rohrer	1-9-2009	Added setElementString method.
*		Jason Rohrer	9-7-2009	Fixed int types.
*									Added appendElementString.
*		Jason Rohrer	11-11-2009	Changed second deleteElement to allow
*                                   SimpleVectors containing ints.
*		Jason Rohrer	12-23-2009	New push_back for arrays.
*		Jason Rohrer	1-5-2010	Reduced default size to conserve memory.
*		Jason Rohrer	1-12-2010	Added copy constructor and assignment 
*                                   operator.
*		Jason Rohrer	1-16-2010	Fixed bugs in new constructor/operator.
*		Jason Rohrer	1-18-2010	Fixed memmove/memcpy bugs.
*		Jason Rohrer	1-28-2010	Data protected for subclass access.
*		Jason Rohrer	4-28-2010	Fast version of getElement.
*		Jason Rohrer	5-14-2010	String parameters as const to fix warnings.
*		Jason Rohrer	11-2-2010	Added appendArray function.
*		Jason Rohrer	11-2-2010	Added appendArray function.
*		Jason Rohrer	2-6-2011	Added support for printing message on
*									vector expansion.
*		Jason Rohrer	2-12-2011	Added push_front function.
*		Jason Rohrer	2-18-2011	Missing member inits found by cppcheck.
*		Jason Rohrer	7-2-2014	Auto-deallocating functions for c-strings.
*		Jason Rohrer	8-5-2014	Added getElementDirect (gets non-pointer).
*		Jason Rohrer	7-15-2015	Fixed getElementDirect type warning.
*		Jason Rohrer	8-25-2015	Fixed getElementDirect to handle structs.
*		Jason Rohrer	5-20-2016	Added functions for getting last element.
*		Jason Rohrer	8-30-2016	Added shrink function.
*		Jason Rohrer	4-12-2017	Added swap function.
*		Jason Rohrer	12-26-2017	deleteLastElement, push_back other vector.
*		Jason Rohrer	1-4-2018	deleteStartElements for efficiency.
*		Jason Rohrer	7-12-2018	push_middle function.
*		Jason Rohrer	5-10-2019	getElementDirectFast function.
*		Jason Rohrer	11-21-2023	getMatchingStringIndex function.
*		Jason Rohrer	1-12-2024   Preventing size-zero vector construction,
*                                   which causes a crash when vector grows
*                                   (can't double zero).
*		Jason Rohrer	3-15-2024   Removed duplicate implementation for
*                                   push_back and appendArray.
*                                   New fast implemention of appendArray using
*	                                memcpy for simple types that don't need
*                                   deep copying.
*                                   New fast implemention of getElementArray 
*                                   using memcpy for simple types that don't
*                                   need deep copying.
*		Jason Rohrer	11-8-2024   Profiling found inefficiency in
*	                                push_back_other after deleting all
*                                   elements in target vector, since vector
*	                                max size would shrink back to 2, and then
*                                   get expanded/copied over and over
*	                                for each push.  Refactored expansion code
*                                   into expandToNewMaxSize, and invoked it in
*                                   push_back_other to make room for other
*                                   vector in one expansion step.
*                                   Function for setting useFastMethods
*                                   manually, and fast version of
*	                                push_back_other that uses memcpy.  This
*                                   makes push_back_other 2x faster.
*/

#include "minorGems/common.h"



#ifndef SIMPLEVECTOR_INCLUDED
#define SIMPLEVECTOR_INCLUDED

#include <string.h>		// for memory moving functions
#include <stdio.h>


const int defaultStartSize = 2;

template <class Type>
class SimpleVector {
	public:
	
		SimpleVector();		// create an empty vector
		SimpleVector(int sizeEstimate); // create an empty vector with a size estimate
		
		~SimpleVector();
        
        // fast methods default to OFF
        // when off, all operations that involve bulk copying are done
        // with element-by-element assignment, which allows for
        // copy-constructors to be invoked.
        //
        // if inUseFastMethods is set to true
        // bulk copy operations use memcpy instead, which is much faster
        // but doesn't invoke copy constructors for each element
        //
        // Note that certain operations automatically use fast methods
        // for vectors of atomic types (ints, chars, floats)
        void toggleFastMethods( char inUseFastMethods );
        

        
        // copy constructor
        SimpleVector( const SimpleVector &inCopy );
        
        // assignment operator
        SimpleVector & operator = (const SimpleVector &inOther );
        


		
		void push_back(Type x);		// add x to the end of the vector

        // add array of elements to the end of the vector
        // an alias for appendArray
		void push_back(Type *inArray, int inLength);		

        // add all elements from other vector
        void push_back_other( SimpleVector<Type> *inOtherVector );
		

        void push_front(Type x);  // add x to the front of the vector (slower)

        // add x to middle of vector, after inNumBefore items, pushing
        // the rest further back
        void push_middle( Type x, int inNumBefore );
        


		Type *getElement(int index);		// get a ptr to element at index in vector

		Type *getElementFast(int index); // no bounds checking

        // get an element directly as a copy on the stack
        // (changes to this returned element will not affect the element in the
        //  vector)
        // This is useful when the vector is storing pointers anyway.
		Type getElementDirect(int index);

        Type getElementDirectFast(int index);  // no bounds checking


        Type *getLastElement() {
            return getElement( size() - 1 );
            }
        
        
        Type getLastElementDirect() {
            return getElementDirect( size() - 1 );
            }
		
		
		int size();		// return the number of allocated elements in the vector
		
		bool deleteElement(int index);		// delete element at an index in vector
        
        // deletes a block of elements from the start
        // way more efficient than calling deleteElement(0) repeatedly
        bool deleteStartElements( int inNumToDelete );
        
		
        void deleteLastElement() {
            deleteElement( size() - 1 );
            }
        
		/**
		 * Deletes a particular element.  Deletes the first element
		 * in vector == to inElement.
		 *
		 * @param inElement the element to delete.
		 *
		 * @return true iff an element was deleted.
		 */
		bool deleteElementEqualTo( Type inElement );


        // shrinks vector, discaring elements beyond inNewSize
        void shrink( int inNewSize );
        
        
        // swaps element at index A with element at index B
        void swap( int inA, int inB );
        


		/**
		 * Gets the index of a particular element.  Gets the index of the
		 * first element in vector == to inElement.
		 *
		 * @param inElement the element to get the index of.
		 *
		 * @return the index if inElement, or -1 if inElement is not found.
		 */
		int getElementIndex( Type inElement );

		
		
		void deleteAll();		// delete all elements from vector


        
		/**
		 * Gets the elements as an array.
		 *
		 * @return the a new array containing all elements in this vector.
         *   Must be destroyed by caller, though elements themselves are
         *   not copied.
		 */
		Type *getElementArray();


        
        /**
		 * Gets the char elements as a \0-terminated string.
		 *
		 * @return a \0-terminated string containing all elements in this
         *   vector.
         *   Must be destroyed by caller.
		 */
		char *getElementString();


        /**
		 * Sets the char elements as a \0-terminated string.
		 *
		 * @param inString a \0-terminated string containing all elements to 
         *   set this vector with.
         *   Must be destroyed by caller.
		 */
		void setElementString( const char *inString );



        /**
		 * Appends chars from a \0-terminated string.
		 *
		 * @param inString a \0-terminated string containing all elements to 
         *   append to this vector.
         *   Must be destroyed by caller.
		 */
		void appendElementString( const char *inString );
        


        /**
		 * Appends elements from an array.
		 *
		 * @param inArray elements to append to this vector.
         *   Must be destroyed by caller.
         * @param inSize the number of elements to append.
		 */
        void appendArray( Type *inArray, int inSize );
        

        
        /**
         * Toggles printing of messages when vector expands itself
         * Defaults to off.
         *
         * @param inPrintMessage true to turn expansion message printing on.
         * @param inVectorName the name to include in the message.
         *   Defaults to "unnamed".
         */
        void setPrintMessageOnVectorExpansion( 
            char inPrintMessage, const char *inVectorName = "unnamed" );
        



        /**
		 * For vectors of char* elements (c-strings).
         *
         * De-allocates a specific char* elements in the vector (by calling 
         * delete[] on it) and deletes it from the vector. 
		 * 
         * Returns true if found and deleted
         */
		char deallocateStringElement( int inIndex );



        /**
		 * For vectors of char* elements (c-strings).
         *
         * De-allocates all char* elements in the vector (by calling delete[] 
         * on each element) and deletes them from the vector. 
		 */
		void deallocateStringElements();

        
        /**
		 * For vectors of char* elements (c-strings).
         *
         * Gets index of first string that matches with strcmp, or
         * -1 if no match found.
         */
        int getMatchingStringIndex( char *inString );



	protected:
		Type *elements;
		int numFilledElements;
		int maxSize;
		int minSize;	// number of allocated elements when vector is empty
        
        char useFastMethods;
        

        char printExpansionMessage;
        const char *vectorName;


        // used when implementing appendArray specialzations for simple
        // types that don't need deep, element-by-element copying
        void appendArrayFast( Type *inArray, int inSize );
        
        // same for fast getElementArray, for simple types
        Type *getElementArrayFast();
        

        void expandToNewMaxSize( int inNewMaxSize );
		};
		
		
template <class Type>		
inline SimpleVector<Type>::SimpleVector()
		: vectorName( "" ) {
	elements = new Type[defaultStartSize];
	numFilledElements = 0;
	maxSize = defaultStartSize;
	minSize = defaultStartSize;
    useFastMethods = false;
    
    printExpansionMessage = false;
    }

template <class Type>
inline SimpleVector<Type>::SimpleVector(int sizeEstimate)
		: vectorName( "" ) {
    if( sizeEstimate <= 0 ) {
        // can't double 0 when vector needs to grow, so min size has to
        // be 1
        sizeEstimate = 1;
        }
    
	elements = new Type[sizeEstimate];
	numFilledElements = 0;
	maxSize = sizeEstimate;
	minSize = sizeEstimate;
    useFastMethods = false;

    printExpansionMessage = false;
    }
	
template <class Type>	
inline SimpleVector<Type>::~SimpleVector() {
	delete [] elements;
	}	



// copy constructor
template <class Type>
inline SimpleVector<Type>::SimpleVector( const SimpleVector<Type> &inCopy )
        : elements( new Type[ inCopy.maxSize ] ),
          numFilledElements( inCopy.numFilledElements ),
          maxSize( inCopy.maxSize ), minSize( inCopy.minSize ),
          useFastMethods( inCopy.useFastMethods ),
          printExpansionMessage( inCopy.printExpansionMessage ),
          vectorName( inCopy.vectorName ) {
    


    if( useFastMethods ) {
        // if these objects contain pointers to stack, etc, this is not 
        // going to work (not a deep copy)
        // because it won't invoke the copy constructors of the objects!
        memcpy( elements, inCopy.elements, sizeof( Type ) * numFilledElements );
        }
    else {    
        for( int i=0; i<inCopy.numFilledElements; i++ ) {
            elements[i] = inCopy.elements[i];
            }
        }
    

    }



// assignment operator
template <class Type>
inline SimpleVector<Type> & SimpleVector<Type>::operator = (
    const SimpleVector<Type> &inOther ) {
    
    // pattern found on wikipedia:

    // avoid self-assignment
    if( this != &inOther )  {
        
        // 1: allocate new memory and copy the elements
        Type *newElements = new Type[ inOther.maxSize ];

        if( useFastMethods ) {
            // again, memcpy  doesn't invoke
            // copy constructor on contained object
            memcpy( newElements, inOther.elements, 
                    sizeof( Type ) * inOther.numFilledElements );
            }
        else {    
            for( int i=0; i<inOther.numFilledElements; i++ ) {
                newElements[i] = inOther.elements[i];
                }
            }
        


        // 2: deallocate old memory
        delete [] elements;
 
        // 3: assign the new memory to the object
        elements = newElements;
        numFilledElements = inOther.numFilledElements;
        maxSize = inOther.maxSize;
        minSize = inOther.minSize;
        }

    // by convention, always return *this
    return *this;
    }






template <class Type>
inline int SimpleVector<Type>::size() {
	return numFilledElements;
	}

template <class Type>
inline Type *SimpleVector<Type>::getElement(int index) {
	if( index < numFilledElements && index >=0 ) {
		return &(elements[index]);
		}
	else return NULL;
	}


template <class Type>
inline Type *SimpleVector<Type>::getElementFast(int index) {
    return &(elements[index]);
	}


template <class Type>
inline Type SimpleVector<Type>::getElementDirect(int index) {
	if( index < numFilledElements && index >=0 ) {
		return elements[index];
		}
    // use 0 instead of NULL here to avoid type warnings
	else {
        Type t = Type();
        return t;
        }
	}
	


template <class Type>
inline Type SimpleVector<Type>::getElementDirectFast(int index) {
    return elements[index];
	}



template <class Type>
inline bool SimpleVector<Type>::deleteElement(int index) {
	if( index < numFilledElements) {	// if index valid for this vector
		
		if( index != numFilledElements - 1)  {	
            // this spot somewhere in middle
		
            

            // memmove NOT okay here, because it leaves shallow copies
            // behind that cause errors when the whole element array is 
            // destroyed.

            /*
			// move memory towards front by one spot
			int sizeOfElement = sizeof(Type);
		
			int numBytesToMove = sizeOfElement*(numFilledElements - (index+1));
		
			Type *destPtr = &(elements[index]);
			Type *srcPtr = &(elements[index+1]);
		
			memmove( (void *)destPtr, (void *)srcPtr, 
                    (unsigned int)numBytesToMove);
            */


            for( int i=index+1; i<numFilledElements; i++ ) {
                elements[i - 1] = elements[i];
                }
			}
			
		numFilledElements--;	// one less element in vector
		return true;
		}
	else {				// index not valid for this vector
		return false;
		}
	}



template <class Type>
inline bool SimpleVector<Type>::deleteStartElements( int inNumToDelete ) {
	if( inNumToDelete <= numFilledElements) {
		
		if( inNumToDelete != numFilledElements)  {	
            

            // memmove NOT okay here, because it leaves shallow copies
            // behind that cause errors when the whole element array is 
            // destroyed.

            for( int i=inNumToDelete; i<numFilledElements; i++ ) {
                elements[i - inNumToDelete] = elements[i];
                }
			}
			
		numFilledElements -= inNumToDelete;
		return true;
		}
	else {				// not enough eleements in vector
		return false;
		}
	}



// special case implementation
// we do this A LOT for unsigned char vectors
// and we can use the more efficient memmove for unsigned chars
template <>
inline bool SimpleVector<unsigned char>::deleteStartElements( 
    int inNumToDelete ) {
	
    if( inNumToDelete <= numFilledElements) {
		
		if( inNumToDelete != numFilledElements)  {

            memmove( elements, &( elements[inNumToDelete] ),
                     numFilledElements - inNumToDelete );
            }
			
		numFilledElements -= inNumToDelete;
		return true;
		}
	else {				// not enough elements in vector
		return false;
		}
	}


// same for signed char vectors
template <>
inline bool SimpleVector<char>::deleteStartElements( 
    int inNumToDelete ) {
	
    if( inNumToDelete <= numFilledElements) {
		
		if( inNumToDelete != numFilledElements)  {

            memmove( elements, &( elements[inNumToDelete] ),
                     numFilledElements - inNumToDelete );
            }
			
		numFilledElements -= inNumToDelete;
		return true;
		}
	else {				// not enough elements in vector
		return false;
		}
	}




template <class Type>
inline bool SimpleVector<Type>::deleteElementEqualTo( Type inElement ) {
	int index = getElementIndex( inElement );
	if( index != -1 ) {
		return deleteElement( index );
		}
	else {
		return false;
		}
	}



template <class Type>
inline void SimpleVector<Type>::shrink( int inNewSize ) {
    numFilledElements = inNewSize;
    }


template <class Type>
inline void SimpleVector<Type>::swap( int inA, int inB ) {
    if( inA == inB ) {
        return;
        }
    
    if( inA < numFilledElements && inA >= 0 &&
        inB < numFilledElements && inB >= 0 ) {
        Type temp = elements[ inA ];
        elements[ inA ] = elements[ inB ];
        elements[ inB ] = temp;
        }
    }




template <class Type>
inline int SimpleVector<Type>::getElementIndex( Type inElement ) {
	// walk through vector, looking for first match.
	for( int i=0; i<numFilledElements; i++ ) {
		if( elements[i] == inElement ) {
			return i;
			}
		}
	
	// no element found
	return -1;
	}



template <class Type>
inline void SimpleVector<Type>::deleteAll() {
	numFilledElements = 0;
	if( maxSize > minSize ) {		// free memory if vector has grown
		delete [] elements;
		elements = new Type[minSize];	// reallocate an empty vector
		maxSize = minSize;
		}
	}


template <class Type>
inline void SimpleVector<Type>::push_back(Type x)	{
	if( numFilledElements < maxSize) {	// still room in vector
		elements[numFilledElements] = x;
		numFilledElements++;
		}
	else {					// need to allocate more space for vector

		int newMaxSize = maxSize << 1;		// double size
        
        expandToNewMaxSize( newMaxSize );
        
		elements[numFilledElements] = x;
		numFilledElements++;	
		}
	}


template <class Type>
inline void SimpleVector<Type>::push_front(Type x)	{
    push_middle( x, 0 );
    }



template <class Type>
inline void SimpleVector<Type>::push_middle( Type x, int inNumBefore )	{

    // first push_back to reuse expansion code
    push_back( x );
    
    // now shift all of the "after" elements forward
    for( int i=numFilledElements-2; i>=inNumBefore; i-- ) {
        elements[i+1] = elements[i];
        }
    
    // finally, re-insert in middle spot
    elements[inNumBefore] = x;
    }



template <class Type>
inline void SimpleVector<Type>::push_back(Type *inArray, int inLength)	{
    appendArray( inArray, inLength );
    }


template <class Type>
inline void SimpleVector<Type>::expandToNewMaxSize( int inNewMaxSize ) {
    int newMaxSize = inNewMaxSize;

    if( printExpansionMessage ) {
        printf( "SimpleVector \"%s\" is expanding itself from %d to %d"
                " max elements\n", vectorName, maxSize, newMaxSize );
        }
    

    // NOTE:  memcpy does not work here, because it does not invoke
    // copy constructors on elements.
    // And then "delete []" below causes destructors to be invoked
    //  on old elements, which are shallow copies of new objects.
    
    Type *newAlloc = new Type[newMaxSize];
    /*
      unsigned int sizeOfElement = sizeof(Type);
      unsigned int numBytesToMove = sizeOfElement*(numFilledElements);
      
      
      // move into new space
      memcpy((void *)newAlloc, (void *) elements, numBytesToMove);
    */

    // must use element-by-element assignment to invoke constructors
    for( int i=0; i<numFilledElements; i++ ) {
        newAlloc[i] = elements[i];
        }
    

    // delete old space
    delete [] elements;
	
    elements = newAlloc;
    maxSize = newMaxSize;    
    }




template <class Type>
inline void SimpleVector<Type>::push_back_other(
    SimpleVector<Type> *inOtherVector ) {

    
    int newMaxSize = numFilledElements + inOtherVector->size();
    

    if( newMaxSize > maxSize ) {
        // not enough room in vector

        // expand it all in one operation now
        
        // otherwise, as we push back individual elements, the
        // vector might need to be expanded over and over
        
        expandToNewMaxSize( newMaxSize );
        }

    
    // we have room in vector
    
    if( useFastMethods ) {
        
        memcpy( &( elements[numFilledElements] ),
                inOtherVector->elements, 
                inOtherVector->numFilledElements * sizeof( Type ) );
        
        numFilledElements += inOtherVector->numFilledElements;
        }
    else {
        // slow method, with element-by-element assignment
        // so that copy constructors can be deployed
        for( int i=0; i<inOtherVector->size(); i++ ) {
            push_back( inOtherVector->getElementDirect( i ) );
            }
        }
    }



template <class Type>
inline Type *SimpleVector<Type>::getElementArray() {
    Type *newAlloc = new Type[ numFilledElements ];

    // shallow copy not good enough!
    /*
    unsigned int sizeOfElement = sizeof( Type );
    unsigned int numBytesToCopy = sizeOfElement * numFilledElements;
    
    // copy into new space
    //memcpy( (void *)newAlloc, (void *)elements, numBytesToCopy );
    */

    // use assignment to ensure that constructors are invoked on element copies
    for( int i=0; i<numFilledElements; i++ ) {
        newAlloc[i] = elements[i];
        }

    return newAlloc;
    }




template <class Type>
inline Type *SimpleVector<Type>::getElementArrayFast() {
    Type *newAlloc = new Type[ numFilledElements ];

    memcpy( newAlloc, elements, numFilledElements * sizeof( Type ) );
    
    return newAlloc;
    }




// various specializations for getElementArray, for simple types, 
// use getElementArrayFast

template <>
inline char *SimpleVector<char>::getElementArray() {
    return getElementArrayFast();
    }

template <>
inline unsigned char *SimpleVector<unsigned char>::getElementArray() {
    return getElementArrayFast();
    }

template <>
inline int *SimpleVector<int>::getElementArray() {
    return getElementArrayFast();
    }

template <>
inline unsigned int *SimpleVector<unsigned int>::getElementArray() {
    return getElementArrayFast();
    }

template <>
inline float *SimpleVector<float>::getElementArray() {
    return getElementArrayFast();
    }

template <>
inline double *SimpleVector<double>::getElementArray() {
    return getElementArrayFast();
    }




template <>
inline char *SimpleVector<char>::getElementString() {
    char *newAlloc = new char[ numFilledElements + 1 ];
    unsigned int sizeOfElement = sizeof( char );
    unsigned int numBytesToCopy = sizeOfElement * numFilledElements;
		
    // memcpy fine here, since shallow copy good enough for chars
    // copy into new space
    memcpy( (void *)newAlloc, (void *)elements, numBytesToCopy );

    newAlloc[ numFilledElements ] = '\0';
    
    return newAlloc;
    }









template <class Type>
inline void SimpleVector<Type>::appendArray( Type *inArray, int inSize ) {
    if( useFastMethods ) {
        appendArrayFast( inArray, inSize );
        }
    else {
        // slow but correct

        // push-back, element-by-element, allows deep copying with copy
        // constructor for types that need that.
        for( int i=0; i<inSize; i++ ) {
            push_back( inArray[i] );
            }
        }
    }




template <class Type>
inline void SimpleVector<Type>::appendArrayFast( Type *inArray, int inSize ) {
    // this implementation expands storage in one step and uses
    // memcpy to insert.  Also uses memcpy to expand vector when needed.
    
    // this only works on simple types that don't need to have copy constructors
    // invoked.

    if( numFilledElements + inSize > maxSize ) {        
        // need to allocate more space for vector
        
        // double size until it is big enough
        int newMaxSize = maxSize * 2;
        while( numFilledElements + inSize > newMaxSize ) {
            newMaxSize *= 2;
            }
        
        if( printExpansionMessage ) {
            printf( "SimpleVector \"%s\" is expanding itself from %d to %d"
                    " max elements\n", vectorName, maxSize, newMaxSize );
            }


        // use memcpy for fast copy into new space
        Type *newAlloc = new Type[newMaxSize];

        memcpy( newAlloc, 
                elements, 
                numFilledElements * sizeof( Type ) );
        
        
        // delete old space
        delete [] elements;
        
        elements = newAlloc;
        maxSize = newMaxSize;
        }


    // we have room in vector
    
    memcpy( &( elements[numFilledElements] ),
            inArray, 
            inSize * sizeof( Type ) );
    
    numFilledElements += inSize;
    }





// various specializations for appendArray, for simple types, 
// use appendArrayFast

template <>
inline void SimpleVector<char>::appendArray( char *inArray, int inSize ) {
    appendArrayFast( inArray, inSize );
    }

template <>
inline void SimpleVector<unsigned char>::appendArray( unsigned char *inArray,
                                                      int inSize ) {
    appendArrayFast( inArray, inSize );
    }

template <>
inline void SimpleVector<int>::appendArray( int *inArray, int inSize ) {
    appendArrayFast( inArray, inSize );
    }

template <>
inline void SimpleVector<unsigned int>::appendArray( unsigned int *inArray,
                                                     int inSize ) {
    appendArrayFast( inArray, inSize );
    }

template <>
inline void SimpleVector<float>::appendArray( float *inArray, int inSize ) {
    appendArrayFast( inArray, inSize );
    }

template <>
inline void SimpleVector<double>::appendArray( double *inArray, int inSize ) {
    appendArrayFast( inArray, inSize );
    }




template <>
inline void SimpleVector<char>::appendElementString( const char *inString ) {
    unsigned int numChars = strlen( inString );
    appendArray( (char*)inString, (int)numChars );
    }


template <>
inline void SimpleVector<char>::setElementString( const char *inString ) {
    deleteAll();

    appendElementString( inString );
    }



template <class Type>
inline void SimpleVector<Type>::setPrintMessageOnVectorExpansion( 
    char inPrintMessage, const char *inVectorName) {
    
    printExpansionMessage = inPrintMessage;
    vectorName = inVectorName;
    }



template <class Type>
inline void SimpleVector<Type>::toggleFastMethods( char inUseFastMethods ) {
    useFastMethods = inUseFastMethods;
    }



template <>
inline char SimpleVector<char*>::deallocateStringElement( int inIndex ) {
    if( inIndex < numFilledElements ) {
        delete [] elements[ inIndex ];
        deleteElement( inIndex );
        return true;
        }
    else {
        return false;
        }
    }




template <>
inline void SimpleVector<char*>::deallocateStringElements() {
    for( int i=0; i<numFilledElements; i++ ) {
		delete [] elements[i];
        }

    deleteAll();
    }




template <>
inline int SimpleVector<char*>::getMatchingStringIndex( char *inString ) {
    for( int i=0; i<numFilledElements; i++ ) {
        if( strcmp( elements[i], inString ) == 0 ) {
            return i;
            }
        }
    return -1;
    }





#endif
