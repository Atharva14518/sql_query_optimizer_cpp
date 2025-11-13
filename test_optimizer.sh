#!/bin/bash

echo "=========================================="
echo "🚀 SQL QUERY OPTIMIZER - INTERACTIVE TEST"
echo "=========================================="
echo ""

# Test queries
declare -a QUERIES=(
    "SELECT * FROM users u, orders o WHERE u.id = o.user_id AND u.age > 25"
    "SELECT u.name, (SELECT COUNT(*) FROM orders WHERE user_id = u.id) FROM users u"
    "SELECT c.name FROM candidate c, party p, district d WHERE c.PartyID = p.PartyID AND c.DistrictID = d.DistrictID"
)

declare -a DESCRIPTIONS=(
    "Comma Join Query"
    "Scalar Subquery (N+1 Problem)"
    "Multi-table Comma Join"
)

echo "Available Test Queries:"
echo ""
for i in "${!QUERIES[@]}"; do
    echo "[$((i+1))] ${DESCRIPTIONS[$i]}"
    echo "    ${QUERIES[$i]}"
    echo ""
done

echo "=========================================="
echo ""
echo "To test with actual MySQL database:"
echo ""
echo "1. Set environment variables:"
echo "   export MYSQL_HOST=localhost"
echo "   export MYSQL_USER=your_user"
echo "   export MYSQL_PASSWORD=your_password"
echo "   export MYSQL_DB=your_database"
echo ""
echo "2. Run optimizer:"
echo "   echo \"${QUERIES[0]}\" | ./sqlopt"
echo ""
echo "Or use non-interactive mode:"
echo "   export MYSQL_NONINTERACTIVE=1"
echo "   echo \"SELECT * FROM users\" | ./sqlopt"
echo ""
echo "=========================================="
echo ""

# Check if we can run a simple test
if [ -f "./sqlopt" ]; then
    echo "📊 EXAMPLE OUTPUT (What you'll see):"
    echo ""
    echo "Connected to MySQL"
    echo "Selected database: your_database"
    echo "Loaded tables:"
    echo "  users (rows: 1000)"
    echo "    - id (int)"
    echo "    - name (varchar)"
    echo "    - age (int)"
    echo ""
    echo "sqlopt> type SQL. Use EXPLAIN prefix to show plan. Ctrl-D to exit."
    echo "sql> SELECT * FROM users u, orders o WHERE u.id = o.user_id AND u.age > 25"
    echo ""
    echo "✅ Query Type: Well-formed Query"
    echo ""
    echo "-- Transform log --"
    echo "1. [comma_join_conversion] Converted comma-separated tables to explicit JOINs"
    echo "2. [predicate_pushdown] Pushed filters to appropriate tables"
    echo "Generated 2 execution plans"
    echo "Selected best plan with cost: 150.5"
    echo ""
    echo "--- Plan ---"
    echo "Project(rows=300, cost=150.5, items=[*])"
    echo "  INNER Join(algo=NESTED, rows=300, cost=150.5)"
    echo "    Scan(table=users AS u, rows=1000, cost=100)"
    echo "    Scan(table=orders AS o, rows=5000, cost=500)"
    echo ""
    echo "--- Optimized SQL ---"
    echo "SELECT * FROM users u INNER JOIN orders o ON u.id = o.user_id WHERE u.age > 25"
    echo ""
    echo "--- Execution Results ---"
    echo "1 | Alice | 30 | 101 | 1 | 150.00"
    echo "1 | Alice | 30 | 102 | 1 | 200.00"
    echo "..."
    echo ""
fi

echo "=========================================="
