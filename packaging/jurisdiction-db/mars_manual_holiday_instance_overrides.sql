-- Materialised bridge rows for Ukraine 2023-2027.
-- The runtime currently reads holiday_instance directly, so these rows keep
-- installs working while the general materialiser catches up with the new
-- first-class Ukraine rule set.

insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
)
select 'UA', 2000001, '2023-01-01', 'Новий рік', 'public', 'uk', null
where not exists (
    select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2023-01-01' and holiday_name = 'Новий рік'
);
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000004, '2023-03-08', 'Міжнародний жіночий день', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2023-03-08' and holiday_name = 'Міжнародний жіночий день');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000005, '2023-04-16', 'Великдень (Пасха)', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2023-04-16' and holiday_name = 'Великдень (Пасха)');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000008, '2023-05-01', 'День праці', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2023-05-01' and holiday_name = 'День праці');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000011, '2023-05-08', 'День пам’яті та перемоги над нацизмом у Другій світовій війні 1939–1945 років', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2023-05-08' and holiday_name = 'День пам’яті та перемоги над нацизмом у Другій світовій війні 1939–1945 років');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000006, '2023-06-04', 'Трійця', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2023-06-04' and holiday_name = 'Трійця');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000012, '2023-06-28', 'День Конституції України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2023-06-28' and holiday_name = 'День Конституції України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000013, '2023-07-28', 'День Української Державності', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2023-07-28' and holiday_name = 'День Української Державності');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000016, '2023-08-24', 'День Незалежності України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2023-08-24' and holiday_name = 'День Незалежності України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000019, '2023-10-01', 'День захисників і захисниць України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2023-10-01' and holiday_name = 'День захисників і захисниць України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000003, '2023-12-25', 'Різдво Христове', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2023-12-25' and holiday_name = 'Різдво Христове');

insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000001, '2024-01-01', 'Новий рік', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2024-01-01' and holiday_name = 'Новий рік');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000004, '2024-03-08', 'Міжнародний жіночий день', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2024-03-08' and holiday_name = 'Міжнародний жіночий день');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000005, '2024-05-05', 'Великдень (Пасха)', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2024-05-05' and holiday_name = 'Великдень (Пасха)');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000008, '2024-05-01', 'День праці', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2024-05-01' and holiday_name = 'День праці');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000011, '2024-05-08', 'День пам’яті та перемоги над нацизмом у Другій світовій війні 1939–1945 років', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2024-05-08' and holiday_name = 'День пам’яті та перемоги над нацизмом у Другій світовій війні 1939–1945 років');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000006, '2024-06-23', 'Трійця', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2024-06-23' and holiday_name = 'Трійця');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000012, '2024-06-28', 'День Конституції України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2024-06-28' and holiday_name = 'День Конституції України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000014, '2024-07-15', 'День Української Державності', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2024-07-15' and holiday_name = 'День Української Державності');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000016, '2024-08-24', 'День Незалежності України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2024-08-24' and holiday_name = 'День Незалежності України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000019, '2024-10-01', 'День захисників і захисниць України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2024-10-01' and holiday_name = 'День захисників і захисниць України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000003, '2024-12-25', 'Різдво Христове', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2024-12-25' and holiday_name = 'Різдво Христове');

insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000001, '2025-01-01', 'Новий рік', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2025-01-01' and holiday_name = 'Новий рік');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000004, '2025-03-08', 'Міжнародний жіночий день', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2025-03-08' and holiday_name = 'Міжнародний жіночий день');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000005, '2025-04-20', 'Великдень (Пасха)', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2025-04-20' and holiday_name = 'Великдень (Пасха)');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000008, '2025-05-01', 'День праці', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2025-05-01' and holiday_name = 'День праці');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000011, '2025-05-08', 'День пам’яті та перемоги над нацизмом у Другій світовій війні 1939–1945 років', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2025-05-08' and holiday_name = 'День пам’яті та перемоги над нацизмом у Другій світовій війні 1939–1945 років');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000006, '2025-06-08', 'Трійця', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2025-06-08' and holiday_name = 'Трійця');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000012, '2025-06-28', 'День Конституції України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2025-06-28' and holiday_name = 'День Конституції України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000014, '2025-07-15', 'День Української Державності', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2025-07-15' and holiday_name = 'День Української Державності');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000016, '2025-08-24', 'День Незалежності України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2025-08-24' and holiday_name = 'День Незалежності України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000019, '2025-10-01', 'День захисників і захисниць України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2025-10-01' and holiday_name = 'День захисників і захисниць України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000003, '2025-12-25', 'Різдво Христове', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2025-12-25' and holiday_name = 'Різдво Христове');

insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000001, '2026-01-01', 'Новий рік', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2026-01-01' and holiday_name = 'Новий рік');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000004, '2026-03-08', 'Міжнародний жіночий день', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2026-03-08' and holiday_name = 'Міжнародний жіночий день');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000005, '2026-04-12', 'Великдень (Пасха)', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2026-04-12' and holiday_name = 'Великдень (Пасха)');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000008, '2026-05-01', 'День праці', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2026-05-01' and holiday_name = 'День праці');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000011, '2026-05-08', 'День пам’яті та перемоги над нацизмом у Другій світовій війні 1939–1945 років', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2026-05-08' and holiday_name = 'День пам’яті та перемоги над нацизмом у Другій світовій війні 1939–1945 років');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000006, '2026-05-31', 'Трійця', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2026-05-31' and holiday_name = 'Трійця');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000012, '2026-06-28', 'День Конституції України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2026-06-28' and holiday_name = 'День Конституції України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000014, '2026-07-15', 'День Української Державності', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2026-07-15' and holiday_name = 'День Української Державності');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000016, '2026-08-24', 'День Незалежності України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2026-08-24' and holiday_name = 'День Незалежності України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000019, '2026-10-01', 'День захисників і захисниць України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2026-10-01' and holiday_name = 'День захисників і захисниць України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000003, '2026-12-25', 'Різдво Христове', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2026-12-25' and holiday_name = 'Різдво Христове');

insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000001, '2027-01-01', 'Новий рік', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2027-01-01' and holiday_name = 'Новий рік');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000004, '2027-03-08', 'Міжнародний жіночий день', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2027-03-08' and holiday_name = 'Міжнародний жіночий день');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000005, '2027-05-02', 'Великдень (Пасха)', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2027-05-02' and holiday_name = 'Великдень (Пасха)');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000008, '2027-05-01', 'День праці', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2027-05-01' and holiday_name = 'День праці');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000011, '2027-05-08', 'День пам’яті та перемоги над нацизмом у Другій світовій війні 1939–1945 років', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2027-05-08' and holiday_name = 'День пам’яті та перемоги над нацизмом у Другій світовій війні 1939–1945 років');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000006, '2027-06-20', 'Трійця', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2027-06-20' and holiday_name = 'Трійця');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000012, '2027-06-28', 'День Конституції України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2027-06-28' and holiday_name = 'День Конституції України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000014, '2027-07-15', 'День Української Державності', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2027-07-15' and holiday_name = 'День Української Державності');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000016, '2027-08-24', 'День Незалежності України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2027-08-24' and holiday_name = 'День Незалежності України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000019, '2027-10-01', 'День захисників і захисниць України', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2027-10-01' and holiday_name = 'День захисників і захисниць України');
insert into holiday_instance(
    jurisdiction_id,
    holiday_id,
    holiday_date,
    holiday_name,
    holiday_class,
    language,
    source_document_id
) select 'UA', 2000003, '2027-12-25', 'Різдво Христове', 'public', 'uk', null where not exists (select 1 from holiday_instance where jurisdiction_id = 'UA' and holiday_date = '2027-12-25' and holiday_name = 'Різдво Христове');
