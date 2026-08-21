#pragma once
// Shadow of lib/RecentBooksStore/RecentBooksStore.h. UITheme.cpp #includes this but never
// actually uses any RecentBooksStore:: symbol (confirmed via grep) -- only the RecentBook
// type name appears, and BaseTheme.h already forward-declares `struct RecentBook;` itself.
// This stub only needs to exist so the #include resolves; the real struct layout is
// irrelevant since drawCorruptSaveNotice()'s path never constructs or reads a RecentBook.
struct RecentBook {
  int placeholder = 0;
};
