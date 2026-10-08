-- Small isolated catalogue for Lab route and browser tests, not production data.
create table jurisdiction(jurisdiction_id text primary key, parent_jurisdiction_id text, jurisdiction_type text, name text);
insert into jurisdiction values
('GB',null,'country','United Kingdom'),
('GB-ENG','GB','subdivision','England'),
('GB-WLS','GB','subdivision','Wales'),
('US',null,'country','United States');
create table timezone_code(timezone_name text, timezone_code integer primary key);
insert into timezone_code values ('Europe/London',1),('America/New_York',2);
create table jurisdiction_location_default(jurisdiction_id text primary key);
create table jurisdiction_location_default_latitude(jurisdiction_id text primary key,latitude text);
create table jurisdiction_location_default_longitude(jurisdiction_id text primary key,longitude text);
create table jurisdiction_location_default_timezone(jurisdiction_id text primary key,timezone_code integer);
create table jurisdiction_location_default_locality(jurisdiction_id text primary key,locality_name text);
insert into jurisdiction_location_default values('GB-ENG'),('GB-WLS'),('US');
insert into jurisdiction_location_default_latitude values('GB-ENG','52.7077'),('GB-WLS','53.3190'),('US','40.7128');
insert into jurisdiction_location_default_longitude values('GB-ENG','-2.7541'),('GB-WLS','-3.4916'),('US','-74.0060');
insert into jurisdiction_location_default_timezone values('GB-ENG',1),('GB-WLS',1),('US',2);
insert into jurisdiction_location_default_locality values('GB-ENG','Shrewsbury'),('GB-WLS','Rhyl'),('US','New York');
create table jurisdiction_town(jurisdiction_town_id integer primary key,jurisdiction_id text,town_name text);
create table jurisdiction_town_latitude(jurisdiction_town_id integer primary key,latitude text);
create table jurisdiction_town_longitude(jurisdiction_town_id integer primary key,longitude text);
create table jurisdiction_town_elevation(jurisdiction_town_id integer primary key,elevation_metres text);
create table jurisdiction_town_timezone(jurisdiction_town_id integer primary key,timezone_code integer);
create table jurisdiction_default_town(jurisdiction_id text primary key,jurisdiction_town_id integer);
insert into jurisdiction_town values(1,'GB-ENG','Shrewsbury'),(2,'GB-WLS','Rhyl'),(3,'US','New York');
insert into jurisdiction_town_latitude values(1,'52.7077'),(2,'53.3190'),(3,'40.7128');
insert into jurisdiction_town_longitude values(1,'-2.7541'),(2,'-3.4916'),(3,'-74.0060');
insert into jurisdiction_town_elevation values(1,'75'),(2,'5'),(3,'10');
insert into jurisdiction_town_timezone values(1,1),(2,1),(3,2);
insert into jurisdiction_default_town values('GB-ENG',1),('GB-WLS',2),('US',3);
