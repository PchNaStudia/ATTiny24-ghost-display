from platformio.public import TestCase, TestRunnerBase, TestStatus


class CustomTestRunner(TestRunnerBase):
    def on_testing_line_output(self, line):
        super().on_testing_line_output(line)
        if line.startswith("PASS:"):
            self.test_suite.add_case(
                TestCase(name=self.test_suite.test_name,
                         status=TestStatus.PASSED, message=line[5:].strip())
            )
        elif line.startswith("FAIL:"):
            self.test_suite.add_case(
                TestCase(name=self.test_suite.test_name,
                         status=TestStatus.FAILED, message=line[5:].strip())
            )