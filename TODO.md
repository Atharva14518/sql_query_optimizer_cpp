# Database Selection TODO

- [x] Modify cli.cpp: Remove direct database prompt, connect without database initially
- [x] After connection, retrieve and display databases numbered 1 to N, filter out system databases
- [x] Prompt user to choose a database by number, validate input
- [x] Select the chosen database using selectDatabase()
- [x] Retrieve and display tables in the chosen database with columns, lowercase table names, sorted columns
- [x] Change prompts to match desired output: sqlopt> and sql>
- [x] Proceed with existing SQL optimizer loop
- [x] Test the CLI to ensure it matches the desired output format

## Previous Testing TODO

- [x] Test simple SELECT: SELECT * FROM users;
- [x] Test SELECT with WHERE: SELECT * FROM users WHERE age = 30;
- [x] Test SELECT with JOIN and WHERE: SELECT u.id, u.name FROM users u JOIN orders o ON u.id = o.user_id WHERE u.age = 30 AND o.status = 'shipped' LIMIT 10;
- [x] Test SELECT with WHERE string: SELECT * FROM users WHERE name = 'Alice';
- [x] Test aggregate: SELECT COUNT(*) FROM users;
- [x] Test GROUP BY: SELECT u.name, COUNT(o.id) FROM users u JOIN orders o ON u.id = o.user_id GROUP BY u.name;
- [x] Test ORDER BY and LIMIT: SELECT * FROM users ORDER BY age LIMIT 5;
- [x] Test multiple JOINs: SELECT u.id, p.name FROM users u JOIN orders o ON u.id = o.user_id JOIN products p ON o.product_id = p.id WHERE u.age > 25;
- [x] Test invalid query: SELECT * FROM;
- [x] Test LIKE: SELECT * FROM users WHERE name LIKE 'A%';
- [x] Test subquery (if supported): SELECT * FROM users WHERE id IN (SELECT user_id FROM orders);
- [x] Test DISTINCT: SELECT DISTINCT age FROM users;

## Issues Found:
- Pushdown only works for qualified columns (with alias.column), not unqualified.
- Extra ) in function calls like COUNT(*))
- GROUP BY parsing doesn't handle dotted identifiers (e.g., GROUP BY u.name parsed as u)
- DISTINCT not included in optimized SQL
- No support for subqueries, IN, OR in WHERE, etc.
- String quotes stripped in output
