#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace util {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
    return s;
}

std::vector<std::string> parseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (quoted) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    current += '"';
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                current += c;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            fields.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    fields.push_back(trim(current));
    return fields;
}

std::string csvEscape(const std::string& s) {
    if (s.find_first_of(",\"\n") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += '"';
        out += c;
    }
    return out + "\"";
}

std::string formatDate(std::time_t t) {
    char buffer[16];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", std::localtime(&t));
    return buffer;
}

std::string today() { return formatDate(std::time(nullptr)); }

std::string datePlusDays(int days) {
    return formatDate(std::time(nullptr) + static_cast<std::time_t>(days) * 86400);
}

int idNumber(const std::string& id) {
    try {
        return id.size() > 1 ? std::stoi(id.substr(1)) : 0;
    } catch (const std::exception&) {
        return 0;
    }
}

std::string makeId(char prefix, int number) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%c%03d", prefix, number);
    return buffer;
}

std::string readLine(const std::string& prompt) {
    std::string line;
    std::cout << prompt;
    if (!std::getline(std::cin, line)) {
        std::cout << "\nInput closed. Exiting.\n";
        std::exit(0);
    }
    return trim(line);
}

std::string readNonEmpty(const std::string& prompt) {
    while (true) {
        std::string value = readLine(prompt);
        if (!value.empty()) return value;
        std::cout << "Input cannot be empty.\n";
    }
}

int readInt(const std::string& prompt, int low, int high) {
    while (true) {
        std::string line = readLine(prompt);
        try {
            size_t pos = 0;
            int value = std::stoi(line, &pos);
            if (pos == line.size() && value >= low && value <= high) return value;
        } catch (const std::exception&) {
        }
        std::cout << "Enter a whole number between " << low << " and " << high << ".\n";
    }
}

}

class Book {
public:
    Book(std::string id, std::string title, std::string author, std::string category, int year)
        : id_(std::move(id)), title_(std::move(title)), author_(std::move(author)),
          category_(std::move(category)), year_(year) {}

    const std::string& id() const { return id_; }
    const std::string& title() const { return title_; }
    const std::string& author() const { return author_; }
    const std::string& category() const { return category_; }
    int year() const { return year_; }
    bool isAvailable() const { return available_; }
    void setAvailable(bool available) { available_ = available; }

    bool matches(const std::string& query) const {
        std::string haystack = util::toLower(title_ + " " + author_ + " " + category_);
        return haystack.find(util::toLower(query)) != std::string::npos;
    }

    std::string toRecord(const std::string& borrowerId, const std::string& dueDate) const {
        return util::csvEscape(id_) + "," + util::csvEscape(title_) + "," +
               util::csvEscape(author_) + "," + util::csvEscape(category_) + "," +
               (year_ > 0 ? std::to_string(year_) : "") + "," +
               (available_ ? "available" : "borrowed") + "," + util::csvEscape(borrowerId) + "," +
               util::csvEscape(dueDate);
    }

    static Book fromFields(std::vector<std::string> fields) {
        fields.resize(8);
        if (fields[0].empty() || fields[1].empty()) throw std::invalid_argument("bad book record");
        int year = 0;
        if (!fields[4].empty()) {
            size_t pos = 0;
            year = std::stoi(fields[4], &pos);
            if (pos != fields[4].size() || year < 0) throw std::invalid_argument("bad year");
        }
        return Book(util::toUpper(fields[0]), fields[1], fields[2], fields[3], year);
    }

private:
    std::string id_;
    std::string title_;
    std::string author_;
    std::string category_;
    int year_;
    bool available_ = true;
};

class Member {
public:
    Member(std::string id, std::string name) : id_(std::move(id)), name_(std::move(name)) {}

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }

    std::string toRecord() const { return util::csvEscape(id_) + "," + util::csvEscape(name_); }

    static Member fromFields(const std::vector<std::string>& fields) {
        if (fields.size() < 2 || fields[0].empty() || fields[1].empty())
            throw std::invalid_argument("bad member record");
        return Member(util::toUpper(fields[0]), fields[1]);
    }

private:
    std::string id_;
    std::string name_;
};

class Loan {
public:
    Loan(std::string bookId, std::string memberId, std::string dueDate)
        : bookId_(std::move(bookId)), memberId_(std::move(memberId)), dueDate_(std::move(dueDate)) {}

    const std::string& bookId() const { return bookId_; }
    const std::string& memberId() const { return memberId_; }
    const std::string& dueDate() const { return dueDate_; }
    bool isActive() const { return returnDate_.empty(); }
    bool isOverdue() const { return isActive() && !dueDate_.empty() && dueDate_ < util::today(); }
    void markReturned(const std::string& date) { returnDate_ = date; }

private:
    std::string bookId_;
    std::string memberId_;
    std::string dueDate_;
    std::string returnDate_;
};

class Library {
public:
    static const int MAX_LOANS = 3;
    static const int LOAN_DAYS = 14;

    Library(std::string booksFile, std::string membersFile)
        : booksFile_(std::move(booksFile)), membersFile_(std::move(membersFile)) {}

    bool load();
    bool save() const;

    std::string addBook(const std::string& title, const std::string& author,
                        const std::string& category, int year);
    std::string addMember(const std::string& name);
    bool issueBook(const std::string& bookId, const std::string& memberId, std::string& message);
    bool returnBook(const std::string& bookId, std::string& message);

    std::vector<const Book*> search(const std::string& query) const;
    std::vector<const Book*> availableBooks() const;
    std::vector<const Book*> allBooks() const;
    std::vector<const Loan*> activeLoans() const;
    int activeLoanCount(const std::string& memberId) const;

    const Book* findBook(const std::string& id) const;
    const Member* findMember(const std::string& id) const;
    const Loan* loanFor(const std::string& bookId) const;
    const std::vector<Member>& members() const { return members_; }

private:
    void loadMembers();
    static bool writeAtomic(const std::string& path, const std::string& content);

    std::string booksFile_;
    std::string membersFile_;
    std::vector<Book> books_;
    std::vector<Member> members_;
    std::vector<Loan> loans_;
};

bool Library::load() {
    std::ifstream in(booksFile_);
    if (!in) return false;
    books_.clear();
    members_.clear();
    loans_.clear();
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        line = util::trim(line);
        if (line.empty() || line[0] == '#') continue;
        try {
            std::vector<std::string> fields = util::parseCsvLine(line);
            if (util::toLower(fields[0]) == "book_id") continue;
            fields.resize(8);
            Book book = Book::fromFields(fields);
            if (findBook(book.id())) throw std::invalid_argument("duplicate book id");
            if (util::toLower(fields[5]) == "borrowed" && !fields[6].empty()) {
                book.setAvailable(false);
                loans_.emplace_back(book.id(), util::toUpper(fields[6]), fields[7]);
            }
            books_.push_back(book);
        } catch (const std::exception&) {
            std::cerr << "Skipping invalid book record on line " << lineNo << "\n";
        }
    }
    loadMembers();
    for (const Loan& l : loans_)
        if (!findMember(l.memberId())) members_.emplace_back(l.memberId(), l.memberId());
    return true;
}

void Library::loadMembers() {
    std::ifstream in(membersFile_);
    if (!in) return;
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        line = util::trim(line);
        if (line.empty() || line[0] == '#') continue;
        try {
            std::vector<std::string> fields = util::parseCsvLine(line);
            if (util::toLower(fields[0]) == "member_id") continue;
            Member member = Member::fromFields(fields);
            if (findMember(member.id())) throw std::invalid_argument("duplicate member id");
            members_.push_back(member);
        } catch (const std::exception&) {
            std::cerr << "Skipping invalid member record on line " << lineNo << "\n";
        }
    }
}

bool Library::writeAtomic(const std::string& path, const std::string& content) {
    std::filesystem::path target(path);
    std::error_code ec;
    if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path(), ec);
    std::filesystem::path temp = target;
    temp += ".tmp";
    {
        std::ofstream out(temp);
        if (!out) return false;
        out << content;
        out.flush();
        if (!out) return false;
    }
    std::filesystem::rename(temp, target, ec);
    return !ec;
}

bool Library::save() const {
    std::ostringstream books;
    books << "book_id,title,author,category,year,status,borrower_id,due_date\n";
    for (const Book& b : books_) {
        const Loan* loan = loanFor(b.id());
        books << b.toRecord(loan ? loan->memberId() : "", loan ? loan->dueDate() : "") << "\n";
    }
    std::ostringstream members;
    members << "member_id,name\n";
    for (const Member& m : members_) members << m.toRecord() << "\n";
    return writeAtomic(booksFile_, books.str()) && writeAtomic(membersFile_, members.str());
}

std::string Library::addBook(const std::string& title, const std::string& author,
                             const std::string& category, int year) {
    int highest = 0;
    for (const Book& b : books_) highest = std::max(highest, util::idNumber(b.id()));
    std::string id = util::makeId('B', highest + 1);
    books_.emplace_back(id, title, author, category, year);
    return id;
}

std::string Library::addMember(const std::string& name) {
    int highest = 0;
    for (const Member& m : members_) highest = std::max(highest, util::idNumber(m.id()));
    std::string id = util::makeId('M', highest + 1);
    members_.emplace_back(id, name);
    return id;
}

bool Library::issueBook(const std::string& bookId, const std::string& memberId,
                        std::string& message) {
    Book* book = nullptr;
    for (Book& b : books_)
        if (b.id() == bookId) book = &b;
    if (!book) {
        message = "No book with ID " + bookId + ".";
        return false;
    }
    const Member* member = findMember(memberId);
    if (!member) {
        message = "No member with ID " + memberId + ".";
        return false;
    }
    if (!book->isAvailable()) {
        message = "'" + book->title() + "' is already borrowed.";
        return false;
    }
    if (activeLoanCount(memberId) >= MAX_LOANS) {
        message = member->name() + " has reached the limit of " + std::to_string(MAX_LOANS) +
                  " active loans.";
        return false;
    }
    std::string due = util::datePlusDays(LOAN_DAYS);
    loans_.emplace_back(bookId, memberId, due);
    book->setAvailable(false);
    message = "Issued '" + book->title() + "' to " + member->name() + ". Due " + due + ".";
    return true;
}

bool Library::returnBook(const std::string& bookId, std::string& message) {
    Book* book = nullptr;
    for (Book& b : books_)
        if (b.id() == bookId) book = &b;
    if (!book) {
        message = "No book with ID " + bookId + ".";
        return false;
    }
    Loan* loan = nullptr;
    for (Loan& l : loans_)
        if (l.isActive() && l.bookId() == bookId) loan = &l;
    if (!loan) {
        message = "'" + book->title() + "' is not currently borrowed.";
        return false;
    }
    bool overdue = loan->isOverdue();
    loan->markReturned(util::today());
    book->setAvailable(true);
    message = "Returned '" + book->title() + "'" + (overdue ? " (it was overdue)." : ".");
    return true;
}

std::vector<const Book*> Library::search(const std::string& query) const {
    std::vector<const Book*> results;
    for (const Book& b : books_)
        if (b.matches(query)) results.push_back(&b);
    return results;
}

std::vector<const Book*> Library::availableBooks() const {
    std::vector<const Book*> results;
    for (const Book& b : books_)
        if (b.isAvailable()) results.push_back(&b);
    return results;
}

std::vector<const Book*> Library::allBooks() const {
    std::vector<const Book*> results;
    for (const Book& b : books_) results.push_back(&b);
    return results;
}

std::vector<const Loan*> Library::activeLoans() const {
    std::vector<const Loan*> results;
    for (const Loan& l : loans_)
        if (l.isActive()) results.push_back(&l);
    return results;
}

int Library::activeLoanCount(const std::string& memberId) const {
    return static_cast<int>(std::count_if(loans_.begin(), loans_.end(), [&](const Loan& l) {
        return l.isActive() && l.memberId() == memberId;
    }));
}

const Book* Library::findBook(const std::string& id) const {
    for (const Book& b : books_)
        if (b.id() == id) return &b;
    return nullptr;
}

const Member* Library::findMember(const std::string& id) const {
    for (const Member& m : members_)
        if (m.id() == id) return &m;
    return nullptr;
}

const Loan* Library::loanFor(const std::string& bookId) const {
    for (const Loan& l : loans_)
        if (l.isActive() && l.bookId() == bookId) return &l;
    return nullptr;
}

namespace {

std::string clip(const std::string& s, size_t width) {
    return s.size() <= width ? s : s.substr(0, width - 1) + "~";
}

void printMenu() {
    std::cout << "\n===== Library Manager =====\n"
              << "1. Add book\n"
              << "2. Add member\n"
              << "3. Issue book\n"
              << "4. Return book\n"
              << "5. Search books\n"
              << "6. List available books\n"
              << "7. List all books\n"
              << "8. List members\n"
              << "9. List active loans\n"
              << "0. Exit\n";
}

void printBooks(const Library& library, const std::vector<const Book*>& books) {
    if (books.empty()) {
        std::cout << "No books found.\n";
        return;
    }
    std::cout << std::left << std::setw(6) << "ID" << std::setw(32) << "Title" << std::setw(26)
              << "Author" << std::setw(18) << "Category" << std::setw(6) << "Year" << "Status\n"
              << std::string(104, '-') << "\n";
    for (const Book* b : books) {
        std::string status = "Available";
        if (!b->isAvailable()) {
            const Loan* loan = library.loanFor(b->id());
            status = "Borrowed";
            if (loan && !loan->dueDate().empty())
                status += " (due " + loan->dueDate() + (loan->isOverdue() ? ", OVERDUE)" : ")");
        }
        std::cout << std::left << std::setw(6) << b->id() << std::setw(32) << clip(b->title(), 30)
                  << std::setw(26) << clip(b->author(), 24) << std::setw(18)
                  << clip(b->category(), 16) << std::setw(6)
                  << (b->year() > 0 ? std::to_string(b->year()) : "-") << status << "\n";
    }
}

void printMembers(const Library& library) {
    if (library.members().empty()) {
        std::cout << "No members registered.\n";
        return;
    }
    std::cout << std::left << std::setw(7) << "ID" << std::setw(30) << "Name" << "Active loans\n"
              << std::string(50, '-') << "\n";
    for (const Member& m : library.members())
        std::cout << std::left << std::setw(7) << m.id() << std::setw(30) << clip(m.name(), 28)
                  << library.activeLoanCount(m.id()) << "\n";
}

void printLoans(const Library& library) {
    std::vector<const Loan*> loans = library.activeLoans();
    if (loans.empty()) {
        std::cout << "No active loans.\n";
        return;
    }
    std::cout << std::left << std::setw(6) << "Book" << std::setw(32) << "Title" << std::setw(24)
              << "Member" << "Due\n"
              << std::string(78, '-') << "\n";
    for (const Loan* l : loans) {
        const Book* book = library.findBook(l->bookId());
        const Member* member = library.findMember(l->memberId());
        std::cout << std::left << std::setw(6) << l->bookId() << std::setw(32)
                  << clip(book ? book->title() : "?", 30) << std::setw(24)
                  << clip(member ? member->name() : l->memberId(), 22)
                  << (l->dueDate().empty() ? "-" : l->dueDate()) << (l->isOverdue() ? "  OVERDUE" : "")
                  << "\n";
    }
}

void persist(const Library& library) {
    if (!library.save()) std::cout << "Warning: could not save data to disk.\n";
}

}

int main(int argc, char* argv[]) {
    std::string booksFile = argc > 1 ? argv[1] : "library_records.csv";
    std::string membersFile = argc > 2 ? argv[2] : "members.csv";
    Library library(booksFile, membersFile);
    if (library.load()) std::cout << "Loaded records from " << booksFile << "\n";
    else std::cout << "No data file found. Starting with an empty library.\n";

    bool running = true;
    while (running) {
        printMenu();
        int choice = util::readInt("Choose an option (0-9): ", 0, 9);
        std::string message;
        switch (choice) {
            case 1: {
                std::string title = util::readNonEmpty("Title: ");
                std::string author = util::readNonEmpty("Author: ");
                std::string category = util::readNonEmpty("Category: ");
                int year = util::readInt("Year: ", 1000, 2100);
                std::cout << "Added book with ID " << library.addBook(title, author, category, year)
                          << ".\n";
                persist(library);
                break;
            }
            case 2: {
                std::string name = util::readNonEmpty("Member name: ");
                std::cout << "Added member with ID " << library.addMember(name) << ".\n";
                persist(library);
                break;
            }
            case 3: {
                std::string memberId = util::toUpper(util::readNonEmpty("Member ID: "));
                std::string bookId = util::toUpper(util::readNonEmpty("Book ID: "));
                bool ok = library.issueBook(bookId, memberId, message);
                std::cout << message << "\n";
                if (ok) persist(library);
                break;
            }
            case 4: {
                std::string bookId = util::toUpper(util::readNonEmpty("Book ID: "));
                bool ok = library.returnBook(bookId, message);
                std::cout << message << "\n";
                if (ok) persist(library);
                break;
            }
            case 5:
                printBooks(library, library.search(util::readNonEmpty("Search title, author or category: ")));
                break;
            case 6: printBooks(library, library.availableBooks()); break;
            case 7: printBooks(library, library.allBooks()); break;
            case 8: printMembers(library); break;
            case 9: printLoans(library); break;
            case 0: running = false; break;
        }
    }
    persist(library);
    std::cout << "Goodbye!\n";
    return 0;
}
