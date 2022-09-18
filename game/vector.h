/**
 * @brief A container template for storing variable lists of items.
 *
 * This template implements a generic container class, consisting of a databuf
 * object holding the data which changes size as the container has elements added
 * to it. This container is similar in principle to std::vector<> but has a few
 * notable differences in semantics.
 *
 * The vector container does not support iterators, and therefore std library functions
 * depending on them (such as sort()) will not work. Instead, the builtin functions
 * for some such features are included in the class.
 *
 * @tparam T the type of the vector members
 */
template <class T>
struct vector
{
    static const int MINSIZE = 8; /**< The minimum number of elements a vector can be created with. */

    T *buf; /**< The array of data values making up the vector's data.*/
    int alen, /**< The allocated length of the vector, including unused space in the buffer. */
        ulen; /**< The used length of the vector, only counting the part of the buffer which has a vector index */

    /**
     * @brief Creates a new vector object.
     *
     * Creates a new vector object of length 0 and an allocated length of 0; the
     * buffer storing the data is not initialized.
     */
    vector() : buf(nullptr), alen(0), ulen(0)
    {
    }

    /**
     * @brief Assigns the vector to another vector by reference.
     *
     * Points `this` vector to another vector passed by reference.
     * The initialized parameters are replaced by the pointer assignment.
     *
     * @param v The vector for this vector to point to.
     */
    vector(const vector &v) : buf(nullptr), alen(0), ulen(0)
    {
        *this = v;
    }

    /**
     * @brief Destroys the vector object.
     *
     * Deletes the vector and frees the space on the heap allocated to store the
     * vector's data.
     */
    ~vector() { shrink(0); if(buf) delete[] (uchar *)buf; }

    /**
     * @brief Sets this vector equal to the passed one.
     *
     * Returns this vector, after copying the values from the passed vector to
     * it. The allocated length of the two vectors are not necessarily equal after
     * copying; however the assigned values will be equal.
     *
     * @param v the vector to copy to this vector
     *
     * @return a vector of type T with the values of v
     */
    vector<T> &operator=(const vector<T> &v)
    {
        shrink(0);
        if(v.length() > alen)
        {
            growbuf(v.length());
        }
        for(int i = 0; i < v.length(); i++)
        {
            add(v[i]);
        }
        return *this;
    }

    /**
     * @brief Adds a reference to the back of the array, with assigned value.
     *
     * Adds a reference to a new element located at the back of the vector.
     * Dynamically constructs a new object on the heap (using the value passed
     * to construct it) and returns the reference to the newly created element.
     *
     * This function's closest equivalents in std::vector<> are push_back() and
     * emplace_back(). However, these functions do not return a reference to the
     * newly created value.
     *
     * The easiest way to replicate this behavior in std::vector is to create
     * the new item at the back of the vector with emplace_back() followed by
     * using back() to get a reference to the newly created vector.
     *
     * @param x the value to assign to the newly created value
     *
     * @return a reference to the newly created object
     */
    T &add(const T &x)
    {
        if(ulen==alen)
        {
            growbuf(ulen+1);
        }
        new (&buf[ulen]) T(x);
        return buf[ulen++];
    }

    /**
     * @brief Adds a reference to the back of the array.
     *
     * Adds a reference to a new element located at the back of the vector.
     * Dynamically allocates an object of type T on the heap (which will be
     * constructed if necessary) and returns the reference to the newly created
     * element.
     *
     * This function has no equivalent in the std::vector<> methods, since iterators
     * are used to represent addresses of values inside standard template library
     * containers. The closest equivalent is manually allocating the pointer on
     * the heap yourself, then pushing it back to a vector containing pointers or
     * references using push_back().
     *
     * @return a reference to the newly created object
     */
    T &add()
    {
        if(ulen==alen)
        {
            growbuf(ulen+1);
        }
        new (&buf[ulen]) T;
        return buf[ulen++];
    }

    /**
     * @brief Duplicates the last element in the array.
     *
     * Copies and assigns to a new element at the end of the vector the value that
     * the previous last member of the vector has. Expands the allocated length
     * of the vector as necessary if there is no room left.
     *
     * @return the value of the entry which was duplicated.
     */
    T &dup()
    {
        if(ulen==alen)
        {
            growbuf(ulen+1);
        }
        new (&buf[ulen]) T(buf[ulen-1]);
        return buf[ulen++];
    }

    /**
     * @brief Moves this vector into the passed vector and vise versa.
     *
     * Moves the values of *this into the vector pointed to by v, and moves the
     * values inside v into *this.
     *
     * @param v the vector to swap with
     */
    void move(vector<T> &v)
    {
        if(!ulen)
        {
            std::swap(buf, v.buf);
            std::swap(ulen, v.ulen);
            std::swap(alen, v.alen);
        }
        else
        {
            growbuf(ulen+v.ulen);
            if(v.ulen)
            {
                std::memcpy(&buf[ulen], (void  *)v.buf, v.ulen*sizeof(T));
            }
            ulen += v.ulen;
            v.ulen = 0;
        }
    }

    /**
     * @brief Returns whether the index passed is valid for this vector.
     *
     * Compares the size of the vector to the size_t given
     *
     * The std::vector equivalent is to compare the desired value to the size()
     * of the vector.
     *
     * @param i the index to validate
     *
     * @return true if the index is in range
     * @return false if the index is out of range
     */
    bool inrange(size_t i) const { return i<size_t(ulen); }

    /**
     * @brief Returns whether the index passed is valid for this vector.
     *
     * Compares the size of the vector to the integer given.
     *
     * The std::vector equivalent is to compare the desired value to the size()
     * of the vector.
     *
     * @param i the index to validate
     *
     * @return true if the index is in range
     * @return false if the index is out of range
     */
    bool inrange(int i) const { return i>=0 && i<ulen; }

    /**
     * @brief Removes and returns the last value.
     *
     * Shortens the end of the array by one and returns the value previously
     * stored at that location.
     *
     * @return the element that was popped off the end
     */
    T &pop() { return buf[--ulen]; }

    /**
     * @brief Returns the last value in the array.
     *
     * The std::vector<> equivalent of this method is back().
     *
     * @return the last element in the array
     */
    T &last() { return buf[ulen-1]; }

    /**
     * @brief Removes the last element from the vector.
     *
     * Deletes the last element of the vector, de-allocating it from the heap.
     *
     * The std::vector<> equivalent of this method is pop_back().
     *
     * @param
     */
    void drop() { ulen--; buf[ulen].~T(); }

    /**
     * @brief Queries whether the vector has no elements.
     *
     * Even if the vector has a large allocated memory size, this function returns
     * true unless there are accessible elements.
     *
     * The std::vector<> equivalent of this method is also called empty().
     *
     * @return true if the vector has no elements
     * @return false if the vector has elements
     */
    bool empty() const { return ulen==0; }

    /**
     * @brief Returns the number of objects the vector can hold.
     *
     * Returns the total allocated size of the vector, in terms of the size of
     * the object it is storing. This is equal to the maximum number of elements
     * the vector can hold without having to expand its allocated space and copy
     * its contents to a new array.
     *
     * The std::vector<> equivalent of this method is also called capacity().
     * @return the number of objects the vector can hold
     */
    int capacity() const { return alen; }

    /**
     * @brief Returns the size of the array.
     *
     * Returns the number of entries in the array.
     * The std::vector<> equivalent of this method is called size()
     *
     * @return the length of the array
     */
    int length() const { return ulen; }

    /**
     * @brief Returns the element at index i.
     *
     * Gets the ith element in the vector. Does not protect against the boundaries
     * of the vector, either below or above.
     *
     * @param i the index to get the element for
     *
     * @return the element at index i
     */
    T &operator[](int i) { return buf[i]; }

    /**
     * @brief Returns a const reference to the element at index i.
     *
     * Gets the ith element in the vector. Does not protect against the boundaries
     * of the vector, either below or above.
     *
     * @param i the index to get the element for
     *
     * @return a const reference to the element at index i
     */
    const T &operator[](int i) const { return buf[i]; }

    /**
     * @brief Transfers ownership of array to return value.
     *
     * The vector's return array is de-assigned from the vector object and passed
     * to the output.
     *
     * **This function will cause a memory leak if the return value is not assigned
     * to a variable. You must then call delete[] on this returned pointer to avoid
     * the memory leak.**
     */
    T *disown() { T *r = buf; buf = nullptr; alen = ulen = 0; return r; }

    /**
     * @brief Removes elements until i are left.
     *
     * The vector's values beyond i are all dropped, leaving a vector of size i
     * as the remainder. Has no effect if the size to shrink to is equal to or
     * larger than the vector was originally.
     *
     * The std::vector equivalent to this function is to call `resize()`.
     *
     * @param i the last index in the array to keep
     */
    void shrink(int i)
    {
        if(!std::is_class<T>::value)
        {
            ulen = i;
        }
        else
        {
            while(ulen>i)
            {
                drop();
            }
        }
    }

    /**
     * @brief Sets the used size of the array to i
     *
     * Sets the used size of the array to i, even if it is negative or larger than
     * the allocated length.
     */
    void setsize(int i) { ulen = i; }

    /**
     * @brief Deletes contents beyond the passed argument.
     *
     * The elements of the vector with an index above n are deleted. This method
     * is intended for vectors with non-array types; use deletearrays() instead
     * for vectors of arrays.
     *
     * @param n the last parameter to keep in the array
     */
    void deletecontents(int n = 0)
    {
        while(ulen > n)
        {
            delete pop();
        }
    }

    /**
     * @brief Deletes array contents beyond the passed argument.
     *
     * The elements of the vector with an index above n are deleted. This method
     * is intended for vectors with array types; use deletcontents() instead
     * for vectors of non-arrays.
     *
     * @param n the last parameter to keep in the array
     */
    void deletearrays(int n = 0)
    {
        while(ulen > n)
        {
            delete[] pop();
        }
    }

    /**
     * @brief Returns a pointer to the vector's internal array.
     *
     * Returns the address of the vector's internal array. If the vector's size
     * changes, this pointer will become invalidated. The std::vector equivalent
     * to this is data().
     *
     * @return A pointer to the vector's internal array.
     */
    T *getbuf() { return buf; }

    /**
     * @brief Returns a pointer to const for the vector's internal array.
     *
     * Returns the address of the vector's internal array. If the vector's size
     * changes, this pointer will become invalidated. This vector points to const,
     * so it cannot change the values it sees.
     *
     * @return A pointer to const for the vector's internal array.
     */
    const T *getbuf() const { return buf; }

    //now unused

    /*
     * @brief returns whether the pointer is inside the vector's internal array
     *
     * Returns whether the pointer passed has an address that falls within
     * the used addresses of the vector. It will return false if the address is
     * only in the allocated buffer, or is not inside the address range at all.
     *
     * @param e the pointer to determine the status of
     *
     * @return true if the address is inside the vector's array
     * @return false if the address is outside the vector's array
     *
    bool inbuf(const T *e) const { return e >= buf && e < &buf[ulen]; }

     **
     * @brief Sorts using the passed function between passed indices.
     *
     * Calls quicksort on `this` array, using the comparison function passed as
     * its first argument. If n = -1, the last element to be sorted is the last
     * element of the array. Other wise, the array is sorted between `i` and `n`.
     *
     * @param fun the function to sort elements with
     * @param i the first element to sort from
     * @param n the last element to sort until
     *
    template<class F>
    void sort(F fun, int i = 0, int n = -1)
    {
        quicksort(&buf[i], n < 0 ? ulen-i : n, fun);
    }

     **
     * @brief Sorts the values of the array in ascending order.
     *
     * Runs the quicksort algorithm to sort the values in the vector in ascending
     * order. The std::vector<> equivalent of this operation is
     * std::sort(vector.begin(), vector.end()).
     *
     * This operation has a time complexity of n*log(n) on average and n^2 in the
     * worst-case scenario.
     *
    void sort() { sort(sortless()); }

     **
     * @brief Sorts the values of the array in ascending order.
     *
     * Runs the quicksort algorithm to sort the values in the vector in ascending
     * order. This function calls sortnameless() which calls sortless(), so the
     * behavior is the same as sort()
     * The std::vector<> equivalent of this operation is
     * std::sort(vector.begin(), vector.end()).
     *
     * This operation has a time complexity of n*log(n) on average and n^2 in the
     * worst-case scenario.
     *
    void sortname() { sort(sortnameless()); }
    */
    /**
     * @brief Expands the array to the requested size.
     *
     * If successful, the pointer to the new data array will be changed, invalidating
     * any existing pointers. Fails silently if the requested size is smaller than the
     * current allocated size.
     *
     * @param sz the new requested size of the array.
     */
    void growbuf(int sz)
    {
        int olen = alen;
        if(alen <= 0)
        {
            alen = max(MINSIZE, sz);
        }
        else
        {
            while(alen < sz)
            {
                alen += alen/2;
            }
        }
        if(alen <= olen)
        {
            return;
        }
        uchar *newbuf = new uchar[alen*sizeof(T)];
        if(olen > 0)
        {
            if(ulen > 0)
            {
                std::memcpy(newbuf, (void *)buf, ulen*sizeof(T));
            }
            delete[] (uchar *)buf;
        }
        buf = (T *)newbuf;
    }

    /**
     * @brief Expands the array by the amount requested.
     *
     * Expands the data array by the amount requested. Silently fails to expand
     * the array if it is already long enough. If this operation succeeds in lengthening
     * the array, all of the pointers to the old array will become invalidated.
     *
     * @return the buffer starting at the old used length for sz entries
     */
    databuf<T> reserve(int sz)
    {
        if(alen-ulen < sz)
        {
            growbuf(ulen+sz);
        }
        return databuf<T>(&buf[ulen], sz);
    }

    /**
     * @brief Expands the array's used length by the amount given.
     *
     * @param sz the amount to increase the size of the array by
     */
    void advance(int sz)
    {
        ulen += sz;
    }

    /**
     * @brief Expands the array given the size of a buffer.
     *
     * @param p the buffer to get the length to expand by
     */
    void addbuf(const databuf<T> &p)
    {
        advance(p.length());
    }

    /**
     * @brief Adds n empty entries to the array.
     *
     * If this operation extends the vector beyond its original allocated length,
     * old array pointers will become invalidated.
     *
     * @param n the number of empty entries to add.
     */
    T *pad(int n)
    {
        T *buf = reserve(n).buf;
        advance(n);
        return buf;
    }

    /**
     * @brief Adds an element to the vector.
     *
     * This function is a wrapper around add() and has no other behavior.
     * The std::vector<> equivalent is either push_back() or emplace_back().
     *
     * @param v the element to add to the vector
     *
     */
    void put(const T &v) { add(v); }

    /**
     * @brief Adds an array to the vector.
     *
     * Adds an array of length n to the end of the vector, reserving the necessary
     * space if required.
     *
     * The std::vector equivalent to
     *
     * ```
     * foo.put(a, len);
     *
     * ```
     *
     * is
     *
     * ```
     * for(int i = 0; i < len; ++i)
     * {
     *     foo.push_back(a[i]);
     * }
     * ```
     *
     * @param v a pointer to the array to add
     * @param n the number of total entries to add
     */
    void put(const T *v, int n)
    {
        databuf<T> buf = reserve(n);
        buf.put(v, n);
        addbuf(buf);
    }

    /**
     * @brief Removes n elements starting at index i.
     *
     * Removes elements (one-indexed) starting with the element after i and continuing
     * for n entries. Shifts entries beyond those removed by copying to lower array
     * indices.
     *
     * @param i the index after which elements are removed
     * @param n the number of elements to remove
     */
    void remove(int i, int n)
    {
        for(int p = i+n; p<ulen; p++)
        {
            buf[p-n] = buf[p];
        }
        ulen -= n;
    }

    /**
     * @brief Removes an element from the vector.
     *
     * Removes the ith element of the vector. If i is greater than the size of the
     * vector, the array is accessed out-of-bounds. The elements beyond the index
     * selected are all moved forward one space, and the element removed is returned
     * by value.
     *
     * The equivalent std::vector<> expression is to get the element's value with
     * operator[] or at() and then to remove the element with erase().
     *
     * @param i the index of the element to remove
     *
     * @return the value of the element removed
     */
    T remove(int i)
    {
        T e = buf[i];
        for(int p = i+1; p<ulen; p++)
        {
            buf[p-1] = buf[p];
        }
        ulen--;
        return e;
    }

    /**
     * @brief Removes the element at index i, non-order preserving.
     *
     * Removes the ith element of the vector, copying the last element into its
     * location. This changes the resulting order of the vector, while avoiding
     * a more costly shift of vector entries from i upwards.
     *
     * @param i the index of the value to remove
     *
     * @return the value of the element removed
     */
    T removeunordered(int i)
    {
        T e = buf[i];
        ulen--;
        if(ulen>0)
        {
            buf[i] = buf[ulen];
        }
        return e;
    }

    /**
     * @brief Finds the argument inside this array.
     *
     * Returns the index of the vector where the argument is first found.
     * If the object is not found, -1 is returned for the index.
     *
     * @param o the object to find in the array
     *
     * @return the index of the object found int the array
     */
    template<class U>
    int find(const U &o)
    {
        for(int i = 0; i < ulen; ++i)
        {
            if(buf[i]==o)
            {
                return i;
            }
        }
        return -1;
    }

    /**
     * @brief Adds an element iff it is not in the list.
     *
     * This method searches for an element with the same value, and adds a new
     * element containing the value passed iff there is no identical element already
     * present. Fails silently if the element already exists in the vector.
     *
     * @param o The object to attempt to add.
     */
    void addunique(const T &o)
    {
        if(find(o) < 0)
        {
            add(o);
        }
    }

    /**
     * @brief Removes element by searching.
     *
     * Attempts to remove an element that compares equal to the argument passed.
     * Only removes the first element found, if multiple identical entries to
     * the argument are found.
     *
     * @param o The object to search for to delete.
     */
    void removeobj(const T &o)
    {
        for(int i = 0; i < static_cast<int>(ulen); ++i)
        {
            if(buf[i] == o)
            {
                int dst = i;
                for(int j = i+1; j < ulen; j++)
                {
                    if(!(buf[j] == o))
                    {
                        buf[dst++] = buf[j];
                    }
                }
                setsize(dst);
                break;
            }
        }
    }

    /**
     * @brief Removes element by searching, non order-preserving.
     *
     * Attempts to remove an element that compares equal to the argument passed,
     * replacing it with the last element. If multiple identical entries to the
     * argument are found, only the first element is removed.
     *
     * This function acts similarly to removeobj() but does not preserve the order
     * of the resulting vector; it is faster however by not having to shift elements
     * i and upward to preserve the order.
     *
     * @param o THe object to search for to delete
     */
    void replacewithlast(const T &o)
    {
        if(!ulen) //do not operate on empty vector
        {
            return;
        }
        for(int i = 0; i < static_cast<int>(ulen-1); ++i) //loop through
        {
            if(buf[i]==o) //compare to passed item
            {
                buf[i] = buf[ulen-1]; //move last element
                break;
            }
            ulen--; //move index down one
        }
    }

    /**
     * @brief Inserts an element at the specified index.
     *
     * Adds an element to the vector at the specified location. The std::vector<>
     * equivalent is also called insert() and has essentially the same semantics,
     * except that std::vector<> returns an iterator and this function returns a
     * copy of the data.
     *
     * @param i index to insert at
     * @param e element to insert
     *
     * @return the value that was inserted into the array
     */
    T &insert(int i, const T &e)
    {
        add(T()); //add to end of vector
        for(int p = ulen-1; p>i; p--) //from end to index, shift elements forwards one
        {
            buf[p] = buf[p-1];
        }
        buf[i] = e; //insert at ith location in buffer
        return buf[i]; //return the value assigned
    }

    /**
     * @brief Inserts n elements at the specified index.
     *
     * Adds n elements to the vector at the specified index. Equivalent to calling
     * insert(int, const T) multiple times with i, i+1, i+2... i+n, except slightly
     * more efficient. The same value e is assigned to all n entries created.
     *
     * @param i index to insert at
     * @param e element to insert
     * @param n number of elements to add
     *
     */
    T *insert(int i, const T *e, int n)
    {
        if(alen-ulen < n) //expand buffer if needed
        {
            growbuf(ulen+n);
        }
        for(int j = 0; j < n; ++j) //add n elements to end
        {
            add(T());
        }
        for(int p = ulen-1; p>=i+n; p--) //for descending elements after i+n (new elements) move upwards by n
        {
            buf[p] = buf[p-n];
        }
        for(int j = 0; j < n; ++j) //set value of n elements to e
        {
            buf[i+j] = e[j];
        }
        return &buf[i]; //return first newly allocated
    }

    /**
     * @brief Reverses the order of the vector.
     *
     * Flips the first n/2 (rounded down) elements with the back n/2 elements,
     * changing the vector's array so that it is in reverse.
     */
    void reverse()
    {
        for(int i = 0; i < static_cast<int>(ulen/2); ++i)
        {
            std::swap(buf[i], buf[ulen-1-i]);
        }
    }

    /**
     * @brief Returns the binary heap parent of the index
     *
     * For this result to make sense, the vector must be put in heap mode using
     * buildheap().
     *
     * @param i the index to query
     */
    static int heapparent(int i) { return (i - 1) >> 1; }

    /**
     * @brief returns the first binary heap child of the index
     *
     * For this result to make sense, the vector must be put in heap mode using
     * buildheap().
     *
     * The second heap child is located at the return value plus one.
     *
     * @param i the index to query
     */
    static int heapchild(int i) { return (i << 1) + 1; }

    /**
     * @brief Puts the vector in heap mode.
     */
    void buildheap()
    {
        for(int i = ulen/2; i >= 0; i--) downheap(i);
    }

    /**
     * @brief Compares up the vector heap.
     *
     * Used when adding a member to the heap. Ensures that upstream members all
     * satisfy the binary heap condition and moves members accordingly.
     *
     * @param i the index to evaluate
     *
     * @return the index representing the same data as the input index
     */
    int upheap(int i)
    {
        float score = heapscore(buf[i]);
        while(i > 0)
        {
            int pi = heapparent(i);
            if(score >= heapscore(buf[pi]))
            {
                break;
            }
            std::swap(buf[i], buf[pi]);
            i = pi;
        }
        return i;
    }

    /**
     * @brief Adds a number to the heap.
     *
     * Adds a new member at the end of the heap, and then compares it with its
     * parents to place it in the correct part of the heap.
     *
     * @param x the object to add to the heap
     */
    T &addheap(const T &x)
    {
        add(x);
        return buf[upheap(ulen-1)];
    }

    /**
     * @brief Checks down the heap for valid children.
     *
     * Moves elements around the heap beneath the given index.
     *
     * @param i index to evaluate
     *
     * @return the index representing the same data as the input index
     */
    int downheap(int i)
    {
        float score = heapscore(buf[i]);
        for(;;)
        {
            int ci = heapchild(i);
            if(ci >= ulen)
            {
                break;
            }
            float cscore = heapscore(buf[ci]);
            if(score > cscore)
            {
               if(ci+1 < ulen && heapscore(buf[ci+1]) < cscore)
               {
                   std::swap(buf[ci+1], buf[i]); i = ci+1;
               }
               else
               {
                   std::swap(buf[ci], buf[i]); i = ci;
               }
            }
            else if(ci+1 < ulen && heapscore(buf[ci+1]) < score)
            {
                std::swap(buf[ci+1], buf[i]); i = ci+1;
            }
            else break;
        }
        return i;
    }

    /**
     * @brief Removes the first element from the heap.
     *
     * Removes the element at the top of the heap. Recalculates the heap structure
     * after removing the top element.
     *
     * @return the value that was removed
     */
    T removeheap()
    {
        T e = removeunordered(0);
        if(ulen)
        {
            downheap(0);
        }
        return e;
    }

    /*
    // All of the below functions are no longer used.

     **
     * @brief finds the key from a hashtable in the vector.
     *
     * the implementation takes advantage of the numerous overloads
     * of the htcmp() function. Note that this means the generic parameter
     * can be one of the following:
     * - const char *
     * - const stringslice
     * - int
     * - GLuint
     *
     * @returns the index of the element if it exists; -1 otherwise
     *
    template<class K>
    int htfind(const K &key)
    {
        for(int i = 0; i < static_cast<int>(ulen); ++i)
        {
            if(htcmp(key, buf[i]))
            {
                return i;
            }
        }
        return -1;
    }

    #define UNIQUE(overwrite, cleanup) \
        for(int i = 1; i < ulen; i++) \
            if(htcmp(buf[i-1], buf[i])) \
            { \
                int n = i; \
                while(++i < ulen) \
                { \
                    if(!htcmp(buf[n-1], buf[i])) \
                    { \
                        overwrite; \
                        n++; \
                    } \
                } \
                cleanup; \
                break; \
            }

     **
     * @brief removes every duplicate **stack allocated value** from the vector.
     *
     * Contents must be initally sorted.
     * Duplicated items get deleted via a call to setsize().
     *
     * **It may leak memory if used with heap allocated and array items.**
     * **see uniquedeletecontents() and uniquedeletearrays() for that case**
     *
     *
    void unique() // contents must be initially sorted
    {
        UNIQUE(buf[n] = buf[i], setsize(n));
    }

     **
     * @brief removes every duplicate **heap-allocated value** from the vector.
     *
     * Duplicated items get deleted via deletecontents().
     * for the equivalent function for stack values see unique().
     * for the equivalent function for array values see uniquedeletearrays().
    *
    void uniquedeletecontents()
    {
        UNIQUE(std::swap(buf[n], buf[i]), deletecontents(n));
    }

     **
     * @brief removes every duplicate **array value** from the vector.
     *
     * Duplicated items get deleted via deletearrays().
     * for the equivalent function for stack values see unique().
     * for the equivalent function for heap-allocated values see uniquedeletecontents().
     *
    void uniquedeletearrays()
    {
        UNIQUE(std::swap(buf[n], buf[i]), deletearrays(n));
    }
    #undef UNIQUE
    */
};
