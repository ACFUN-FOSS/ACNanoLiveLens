#ifndef NANOLIVELENS_TEST_WIN_HXX
#define NANOLIVELENS_TEST_WIN_HXX

class TestWin
{
public:
    TestWin();
    ~TestWin();

    TestWin(const TestWin &) = delete;
	TestWin(TestWin &&) = delete;
    TestWin &operator=(const TestWin &) = delete;
    TestWin &operator=(TestWin &&) = delete;

private:
    class Impl;
    stdx::pimpl::unique_ptr<Impl> pImpl;
};

#endif
