-- Administrative membership and representative locations share the installed town catalogue.
insert into jurisdiction(jurisdiction_id,parent_jurisdiction_id,jurisdiction_type,
                         iso_country_code,iso_subdivision_code,cldr_region_code,name)
select 'RU-KGD','RU','subdivision','RU','RU-KGD','RU','Kaliningrad Oblast'
where exists(select 1 from jurisdiction where jurisdiction_id='RU')
  and not exists(select 1 from jurisdiction where jurisdiction_id='RU-KGD');

update jurisdiction_town_jurisdiction_id set jurisdiction_id='RU-KGD'
where jurisdiction_id='RU' and jurisdiction_town_id in (
    select t.jurisdiction_town_id from jurisdiction_town_timezone t
    join timezone_code z using(timezone_code) where z.timezone_name='Europe/Kaliningrad');

insert or replace into jurisdiction_default_town(jurisdiction_id,jurisdiction_town_id)
select 'RU-KGD',jurisdiction_town_id from jurisdiction_town where jurisdiction_id='RU-KGD' and town_name='Kaliningrad';
insert or replace into jurisdiction_default_town(jurisdiction_id,jurisdiction_town_id)
select 'RU',jurisdiction_town_id from jurisdiction_town where jurisdiction_id='RU' and town_name='Moscow';

insert or ignore into jurisdiction_location_default(jurisdiction_id)
select jurisdiction_id from jurisdiction where jurisdiction_id='RU-KGD';
insert or replace into jurisdiction_location_default_latitude(jurisdiction_id,latitude)
select d.jurisdiction_id,l.latitude from jurisdiction_default_town d
join jurisdiction_town_latitude l using(jurisdiction_town_id) where d.jurisdiction_id in ('RU','RU-KGD');
insert or replace into jurisdiction_location_default_longitude(jurisdiction_id,longitude)
select d.jurisdiction_id,l.longitude from jurisdiction_default_town d
join jurisdiction_town_longitude l using(jurisdiction_town_id) where d.jurisdiction_id in ('RU','RU-KGD');
insert or replace into jurisdiction_location_default_timezone(jurisdiction_id,timezone_code)
select d.jurisdiction_id,t.timezone_code from jurisdiction_default_town d
join jurisdiction_town_timezone t using(jurisdiction_town_id) where d.jurisdiction_id in ('RU','RU-KGD');
insert or replace into jurisdiction_location_default_locality(jurisdiction_id,locality_name)
select d.jurisdiction_id,t.town_name from jurisdiction_default_town d
join jurisdiction_town t using(jurisdiction_town_id) where d.jurisdiction_id in ('RU','RU-KGD');
