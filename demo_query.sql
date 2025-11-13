-- UNOPTIMIZED QUERY: Using comma joins and scalar subqueries
SELECT 
    c.name,
    (SELECT PartyName FROM party p WHERE p.PartyID = c.PartyID) AS PartyName,
    (SELECT DistrictName FROM district d WHERE d.DistrictID = c.DistrictID) AS DistrictName
FROM candidate c, electionwinner ew, election e
WHERE c.CandidateID = ew.CandidateID
  AND ew.ElectionID = e.ElectionID
  AND c.age > 30
  AND e.ElectionYear = 2024
LIMIT 10;
