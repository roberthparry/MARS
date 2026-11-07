-- Keep Northern Cyprus's mapped control boundary separate from its country identifier.
-- The regional flag layer supplies its display flag; the parent jurisdiction remains Cyprus.
update country_name set iso_country_code='CY'
where name='Turkish Republic of Northern Cyprus' and iso_country_code='';

-- Keep the surveyed polygon edges; identify Russia's detached western component separately.
insert into country_name(id,name,iso_country_code)
select coalesce(max(id),0)+1,'Kaliningrad Oblast','RU-KGD' from country_name
having exists(select 1 from country_name where iso_country_code='RU')
   and not exists(select 1 from country_name where iso_country_code='RU-KGD');

update country_chunk set country=(select id from country_name where iso_country_code='RU-KGD')
where country in (select id from country_name where iso_country_code='RU')
  and id in (select id from country_bounds
             where west>=19000000 and east<=23000000 and south>=54000000 and north<=56000000);
