import localUnitTest from './LocalUnit.test';
import petHomeValidatorsTest from './PetHomeValidators.test';

export default function testsuite() {
  localUnitTest();
  petHomeValidatorsTest();
}
