EXPLAIN SELECT u.id, u.name FROM users u JOIN orders o ON u.id = o.user_id WHERE u.age = 30 AND o.status = 'shipped' LIMIT 10;

EXPLAIN SELECT * FROM users WHERE name = 'Alice';
