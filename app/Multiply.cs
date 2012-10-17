using System;

namespace EmbedSample
{
	public class Operation : IOperation
	{
		public string Name { get { return "Multiply"; } }
		public double Execute(double a, double b)
		{
			return a * b;
		}
	}
}
