#!/bin/bash

echo "Testing false positives fix..."

# Test case 1: User incorrectly claims no more solutions exist
echo "Test 1: a + b = 3, a = 0, b = 3."
echo -e "a + b = 3\na = 0\nb = 3." | timeout 5 ./laure --norepl

echo -e "\n---\n"

# Test case 2: User correctly continues searching
echo "Test 2: a + b = 3, a = 0, b = 3;"
echo -e "a + b = 3\na = 0\nb = 3;" | timeout 5 ./laure --norepl

echo -e "\n---\n"

# Test case 3: 1 + 2 = a, a = 3. (correctly claims no more solutions)
echo "Test 3: 1 + 2 = a, a = 3."
echo -e "1 + 2 = a\na = 3." | timeout 5 ./laure --norepl

echo -e "\n---\n"

# Test case 4: 1 + 2 = a, a = 3; (continues searching - should find 'false')
echo "Test 4: 1 + 2 = a, a = 3;"
echo -e "1 + 2 = a\na = 3;" | timeout 5 ./laure --norepl