#!/bin/bash

# Test validation of false positives in laurelang
# This script tests if typing '.' correctly validates completeness claims

echo "Testing validation mechanism..."
echo ""
echo "Instructions:"
echo "1. Type: a + b = 5"
echo "2. Wait for first solution (a = 0, b = 5)"
echo "3. Press Enter to continue"
echo "4. Wait for second solution (a = 1, b = 4)"
echo "5. Type '.' to claim no more solutions"
echo "6. System should return 'false' because more solutions exist"
echo ""
echo "Starting laurelang REPL..."

./laure